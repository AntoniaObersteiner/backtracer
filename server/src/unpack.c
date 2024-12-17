#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "block.h"

#define BLOCK_ARRAY_CAPACITY 64
const bool dbg = true;

unsigned long get_block_line (
	char ** block_line,
	FILE * file,
	unsigned long * line_number,
	const char * block_marker
) {
	unsigned long line_buffer_capacity = 1023; // +1 for '\0'
	long line_buffer_filled = 0;

	// is allocated once, reallocated when needed by getline, freed never
	static char * line_buffer;
	static bool line_buffer_initialized = false;
	if (!line_buffer_initialized) {
		line_buffer = (char *) malloc(line_buffer_capacity + 1);
		line_buffer_initialized = true;
	}

	do {
		line_buffer_filled = getline(&line_buffer, &line_buffer_capacity, file);
		if (line_buffer_filled == 0) {
			printf("got no line from input file!\n");
			return 0;
		} else if (line_buffer_filled < 0) {
			printf("got return code %ld!\n", line_buffer_filled);
			perror("getline error");
			return 0;
		}

		// replace '\n' by '\0' for printing purposes
		line_buffer[line_buffer_filled - 1] = '\0';
		(*line_number) ++;

		if (dbg) printf("line %4ld: '%s'\n", *line_number, line_buffer);

		if (line_buffer_filled < strlen(block_marker))
			continue;

		for (unsigned long i = 0; i < line_buffer_filled - strlen(block_marker); i++) {
			// if (dbg) printf("i == %3ld / %3ld!\n", i, line_buffer_filled);
			if (0 == strncmp(line_buffer + i, block_marker, strlen(block_marker))) {
				// we found the block
				*block_line = line_buffer + i;
				return line_buffer_filled - i;
			}
		}
	} while (line_buffer_filled); // while we still get bytes
	if (dbg) printf("no line after line number %4ld\n", *line_number);
}

bool is_hex(char c) {
	return (
		('0' <= c && c <= '9') ||
		('a' <= c && c <= 'f')
	);
}
bool from_hex(unsigned long * result, char source) {
	char value;
	if        ('0' <= source && source <= '9') {  value = source - '0';
	} else if ('a' <= source && source <= 'f') {  value = source - 'a' + 10;
	} else return false;

	(*result) = ((*result) << 4) | value;
	return true;
}

bool to_mword(const char * buffer, unsigned long * result, unsigned long * to_skip, unsigned long read_chars) {
	*result = 0;
	for (unsigned long i = 0; i < read_chars; i++) {
		if (!from_hex(result, buffer[i])) {
			*to_skip = i + 1; // where to try again
			return false;
		}
	}
	*to_skip = read_chars;
	return true;
}

void add_to_raw_block_data(
	unsigned long * block_buffer,
	unsigned long * block_buffer_filled,
	const char * line_buffer,
	const unsigned long line_buffer_filled
) {
	// lines look like this (parentheses not literal): "(marker) [(block id):(word)] (data) (data) ..."
	unsigned long   data_start_index = 0;
	unsigned long     id_start_index = 0;
	unsigned long offset_start_index = 0;
	for (unsigned long i = 0; i < line_buffer_filled; i++) {
		if (line_buffer[i] == '[') {
			// spaces after this will abort
			id_start_index = i + 1;
		}
		if (line_buffer[i] == '.') {
			// spaces after this will abort
			offset_start_index = i + 1;
		}
		if (line_buffer[i] == ']') {
			// non-number characters after this are not a problem, because the reader repeats with skip.
			data_start_index = i + 1;
			break;
		}
	}

	unsigned long to_skip;

	{
		unsigned long id;
		if (!to_mword(line_buffer + id_start_index, &id, &to_skip, block_format_id_chars)) {
			printf("could not read id in line markers starting at %ld in '%s'!\n", id_start_index, line_buffer);
			exit(1);
		}
		if (*block_buffer_filled > 0) {
			// otherwise, the block_t block_buffer has not read an id yet to compare
			block_t * block = (block_t *) block_buffer;
			if (block->id != id) {
				printf("have block with id in data %ld, but id in line markers %ld!\n", block->id, id);
				exit(1);
			}
		}
	}

	{
		unsigned long offset;
		if (!to_mword(line_buffer + offset_start_index, &offset, &to_skip, block_format_offset_chars)) {
			printf("could not read offset in line markers starting at %ld in '%s'!\n", offset_start_index, line_buffer);
			exit(1);
		}
		if (*block_buffer_filled != offset) {
			printf("have offset in block %ld, but offset in line markers %ld!\n", *block_buffer_filled, offset);
			exit(1);
		}
	}

	unsigned long i = data_start_index;
	for (; i < line_buffer_filled - 16; ) {
		bool got_word = to_mword(
			line_buffer + i,
			block_buffer + *block_buffer_filled,
			&to_skip,
			16
		);
		if (got_word) {
			if (0) printf(
				"written word to %p (%ld): %016lx\n",
				block_buffer + *block_buffer_filled,
				*block_buffer_filled,
				block_buffer[*block_buffer_filled]
			);
			(*block_buffer_filled) ++;
		}
		i += to_skip;
	}
}

block_t * get_free_block(
	block_t * blocks,
	bool * block_used,
	unsigned long block_array_capacity
) {
	for (unsigned long b = 0; b < block_array_capacity; b++) {
		if (!block_used[b])
			return blocks + b;
	}

	return 0;
}

void recover_block(
	block_t * blocks,
	bool * block_used,
	unsigned long block_array_capacity,
	const block_t ** reorder,
	unsigned long reorder_capacity,
	const block_t * redundancy_block,
	unsigned long block_id_start
) {
	block_t * recovered_block = get_free_block(blocks, block_used, block_array_capacity);
	if (!recovered_block) {
		printf("there is no capacity for the missing block to be recovered!\n");
		exit(1);
	}
	recovered_block->id = -1; // start with invalid id
	for (unsigned long d = 0; d < block_data_capacity_in_words; d++) {
		recovered_block->data[d] = 0;
	}
	for (unsigned long r = 0; r < reorder_capacity; r++) {
		unsigned long id = r + block_id_start;
		if (reorder[r]) {
			printf (
				"recovered_block: id %016lx, len %016lx, flags %016lx, data:\n"
				"%016lx %016lx %016lx %016lx\n%016lx %016lx %016lx %016lx\n",
				recovered_block->id, recovered_block->data_length_in_words, recovered_block->flags,
				recovered_block->data[0], recovered_block->data[1], recovered_block->data[2], recovered_block->data[3],
				recovered_block->data[4], recovered_block->data[5], recovered_block->data[6], recovered_block->data[7]
			);
			xor_blocks(recovered_block, reorder[r]);
			continue;
		}
		if (recovered_block->id != -1) {
			printf(
				"found block (r = %ld, id = %ld) missing, "
				"but id = %ld was already missing?\n",
				r, id, recovered_block->id
			);
			exit(1);
		}

		printf(
			"recovering block (r = %ld, id = %ld, idx = %ld)\n",
			r, id, recovered_block - blocks
		);
		recovered_block->id = id;
		reorder[r] = recovered_block;
		recovered_block->data_length_in_words = block_data_capacity_in_words;
		// free guess. TODO?
		// maybe some 0-detection for the end of the block?
		// or smart reading of content...
	}
	printf (
		"recovered_block: id %016lx, len %016lx, flags %016lx, data:\n"
		"%016lx %016lx %016lx %016lx\n%016lx %016lx %016lx %016lx\n",
		recovered_block->id, recovered_block->data_length_in_words, recovered_block->flags,
		recovered_block->data[0], recovered_block->data[1], recovered_block->data[2], recovered_block->data[3],
		recovered_block->data[4], recovered_block->data[5], recovered_block->data[6], recovered_block->data[7]
	);
	xor_blocks(recovered_block, redundancy_block);
	printf (
		"recovered_block: id %016lx, len %016lx, flags %016lx, data:\n"
		"%016lx %016lx %016lx %016lx\n%016lx %016lx %016lx %016lx\n",
		recovered_block->id, recovered_block->data_length_in_words, recovered_block->flags,
		recovered_block->data[0], recovered_block->data[1], recovered_block->data[2], recovered_block->data[3],
		recovered_block->data[4], recovered_block->data[5], recovered_block->data[6], recovered_block->data[7]
	);
}

unsigned long make_reorder(
	const block_t *** reorder,
	unsigned long * reorder_capacity,
	block_t * blocks,
	bool * block_used,
	unsigned long block_array_capacity,
	unsigned long block_id_start
) {
	unsigned long max_id = 0;
	unsigned long block_use_count = 0;
	for (unsigned long b = 0; b < block_array_capacity; b++) {
		if (!block_used[b])
			continue;

		block_use_count++;
		if (blocks[b].id > max_id) {
			max_id = blocks[b].id;
		}
	}
	unsigned long rel_max_id = max_id - block_id_start;
	unsigned long reorder_capacity_needed = rel_max_id + 1;

	printf("max_id %ld among %ld blocks, %ld read blocks\n", max_id, reorder_capacity_needed, block_use_count);
	if (reorder_capacity_needed > *reorder_capacity) {
		*reorder = (const block_t **) realloc((void *) reorder, reorder_capacity_needed * sizeof(block_t *));
		*reorder_capacity = reorder_capacity_needed;
		if (!*reorder) {
			perror("realloc reorder");
			exit(1);
		}
	}

	for (unsigned long r = 0; r <= rel_max_id; r++) {
		(*reorder)[r] = 0;
	}

	block_t * redundancy_block = 0;
	unsigned long missing = reorder_capacity_needed; // -- for each found block
	for (unsigned long b = 0; b < block_array_capacity; b++) {
		if (!block_used[b])
			continue;

		unsigned long id = blocks[b].id;
		if (id < block_id_start) {
			printf(
				"ignoring block with id %ld from old section "
				"(start of current section is %ld)!\n",
				id, block_id_start
			);
			continue;
		}
		if (blocks[b].flags & BLOCK_REDUNDANCY) {
			printf("redundancy block (id = %ld, idx = %ld).\n", id, b);
			if (blocks[b].id == block_id_start)
				redundancy_block = &(blocks[b]);
			continue;
		}

		if ((*reorder)[id - block_id_start]) {
			printf(
				"error: non-redundancy block (id = %ld, idx = %ld) has same id as previous block (idx = %ld)\n",
				id, b, (*reorder)[id - block_id_start] - blocks
			);
			continue;
		}

		missing--;
		(*reorder)[id - block_id_start] = &(blocks[b]);
		printf("reorder id %ld -> idx %ld\n", id, b);
	}

	printf(
		"%ld blocks missing from section of %ld blocks among %ld noted blocks "
		"(the latter including the redundancy block)!\n",
		missing, reorder_capacity_needed, block_use_count
	);

	if (missing > 1) {
		printf("there are %ld blocks missing. that is beyond the error correction implemented!\n", missing);
		exit(1);
	}

	if (missing == 1) {
		if (!redundancy_block) {
			printf("we want to recover, but have no redundancy block?\n");
			exit(1);
		}
		recover_block(
			blocks, block_used, block_array_capacity,
			*reorder, *reorder_capacity,
			redundancy_block, block_id_start
		);
		block_use_count++;
	}

	block_used[redundancy_block - blocks] = false;

	return reorder_capacity_needed;
}

void write_block_data(
	FILE * output_file,
	const block_t ** blocks,
	unsigned long blocks_filled
) {
	for (unsigned long r = 0; r < blocks_filled; r++) {
		if (!blocks[r]) {
			printf("cannot write missing block %ld!\n", r);
			continue;
		}

		printf("writing block %ld: %p (%ld words) to output\n", r, blocks[r], blocks[r]->data_length_in_words);
		for (unsigned long i = 0; i < blocks[r]->data_length_in_words; i += 4) {
			printf(
				"words at %p: %016lx %016lx %016lx %016lx\n",
				&(blocks[r]->data[i]),
				blocks[r]->data[i + 0],
				blocks[r]->data[i + 1],
				blocks[r]->data[i + 2],
				blocks[r]->data[i + 3]
			);
		}
		unsigned int output_written = fwrite(
			blocks[r]->data,
			sizeof(unsigned long),
			blocks[r]->data_length_in_words,
			output_file
		);
		if (!output_written) {
			if (ferror(output_file)) {
				perror("writing block to output_file");
				exit(1);
			} else {
				perror("no output written to output_file?");
			}
		}
	}
}

int main(int argc, char * argv []) {
	// TODO: use argp

	if (argc <= 2) {
		printf("no input and output filenames given!\n");
		exit(1);
	}

	const char * input_filename = argv[1];
	const char * output_filename = argv[2];
	printf("analyzing '%s'\n", input_filename);
	printf("writing to '%s'\n", output_filename);

	int input_fd, output_fd;
	bool close_input_fd_at_end = false;
	bool close_output_fd_at_end = false;
	if (strcmp(input_filename, "-") == 0) {
		input_fd = STDIN_FILENO;
	} else {
		input_fd = open(input_filename, O_RDONLY);
		printf("input_fd: %d\n", input_fd);
		if (input_fd < 0) {
			perror(input_filename);
			exit(1);
		}
		close_input_fd_at_end = true;
	}

	if (strcmp(output_filename, "-") == 0) {
		printf(
			"the programm will not vomit raw bytes to stdout. "
			"please provide a real filename!\n"
		);
		exit(1);
	} else {
		output_fd = open(
			output_filename,
			O_WRONLY |
			O_CREAT,
			S_IRUSR |
			S_IWUSR |
			S_IRGRP |
			S_IWGRP |
			S_IROTH
		);
		printf("output_fd: %d\n", output_fd);
		if (output_fd < 0) {
			perror(output_filename);
			exit(1);
		}
		close_output_fd_at_end = true;
	}

	FILE * input_file = fdopen(input_fd, "r");
	FILE * output_file = fdopen(output_fd, "w");

	char * line_buffer;
	unsigned long line_buffer_filled;
	unsigned long line_number = 0;

	const unsigned long block_array_capacity = BLOCK_ARRAY_CAPACITY;
	block_t blocks [BLOCK_ARRAY_CAPACITY];
	bool block_used [BLOCK_ARRAY_CAPACITY] = { false };

	block_t * current_block = &blocks[0];
	// how many words do we already have towards the next block
	unsigned long block_buffer_filled = 0;

	// maps block ids starting from block_id_start to their blocks.
	// null entries mark missing blocks
	unsigned long reorder_block_id_start;
	// make_reorder will realloc and change this value
	unsigned long reorder_capacity = BLOCK_ARRAY_CAPACITY;
	const block_t ** reorder = (const block_t **) malloc(sizeof(const block_t *) * reorder_capacity);

	while (true) {
		// find a line with the specified marker in the input. replace \n by \0
		line_buffer_filled = get_block_line(
			&line_buffer,
			input_file,
			&line_number,
			block_marker_default
		);
		if (!line_buffer_filled)
			// we got no data from input, no more input exists
			// TODO: does this handle incomplete blocks?
			// => they should not exist, I believe
			break;

		add_to_raw_block_data(
			(unsigned long *) current_block,
			&block_buffer_filled,
			line_buffer,
			line_buffer_filled
		);

		if (block_buffer_filled < sizeof(block_t) / sizeof(unsigned long))
			continue;

		unsigned long block_index = current_block - &blocks[0];
		printf(
			"filled block idx = %ld, id = %ld (@%p) with %ld words\n",
			block_index, blocks[block_index].id, current_block, block_buffer_filled
		);
		block_used[block_index] = true;

		// the block we just completed
		block_t * block = current_block;
		current_block = get_free_block(blocks, block_used, block_array_capacity);
		block_buffer_filled = 0;

		if (!(block->flags & BLOCK_REDUNDANCY))
			// get next block, we are not at the checking stage yet.
			continue;

		// the redundancy block has the index of the first block its redundancy covers
		long block_id_start = block->id;
		unsigned long reorder_filled = make_reorder(
			&reorder, &reorder_capacity,
			blocks, block_used, block_array_capacity,
			block_id_start
		);

		printf("write blocks\n");
		write_block_data(output_file, reorder, reorder_filled);

		for (unsigned long r = 0; r < reorder_filled; r++) {
			unsigned long index = reorder[r] - &blocks[0];
			block_used[index] = false;
		}
		printf("done\n");
	}

	// the check is for stdin, this code is not correct, but we don't need it anymore anyways...
	if (close_input_fd_at_end) {
		fclose(input_file);
		close(input_fd);
	}
	if (close_output_fd_at_end) {
		printf("running!\n");
		fclose(output_file);
		close(output_fd);
	}
}
