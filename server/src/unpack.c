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
	long line_buffer_filled = 0; // +1 for '\0'
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
	if      ('0' <= source && source <= '9') {
		value = source - '0';
	} else if ('a' <= source && source <= 'f') {
		value = source - 'a' + 10;
	} else {
		return false;
	}

	(*result) = ((*result) << 4) | value;
	return true;
}

bool to_mword(const char * buffer, unsigned long * result, unsigned long * to_skip) {
	*result = 0;
	for (unsigned long i = 0; i < 16; i++) {
		if (!from_hex(result, buffer[i])) {
			*to_skip = i + 1; // where to try again
			return false;
		}
	}
	*to_skip = 16;
	return true;
}

void add_to_raw_block_data(
	unsigned long * block_buffer,
	unsigned long * block_buffer_filled,
	const char * line_buffer,
	const unsigned long line_buffer_filled
) {
	unsigned long start_index = 0;
	for (unsigned long i = 0; i < line_buffer_filled; i++) {
		if (line_buffer[i] == ':') {
			start_index = i + 1;
			break;
		}
	}

	unsigned long i = start_index;
	unsigned long to_skip;
	for (; i < line_buffer_filled - 16; ) {
		bool got_word = to_mword(
			line_buffer + i,
			block_buffer + *block_buffer_filled,
			&to_skip
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

void recover_block(
	block_t * blocks,
	const block_t ** reorder,
	const block_t * redundancy_block,
	unsigned long block_array_filled
) {
	block_t * recovered_block = &(blocks[block_array_filled]);
	recovered_block->id = -1; // start with invalid id
	for (unsigned long d = 0; d < block_data_capacity_in_words; d++) {
		recovered_block->data[d] = 0;
	}
	for (unsigned long b = 0; b < block_array_filled; b++) {
		if (reorder[b]) {
			printf (
				"recovered_block: id %016lx, len %016lx, flags %016lx, data:\n"
				"%016lx %016lx %016lx %016lx\n"
				"%016lx %016lx %016lx %016lx\n",
				recovered_block->id,
				recovered_block->data_length_in_words,
				recovered_block->flags,
				recovered_block->data[0],
				recovered_block->data[1],
				recovered_block->data[2],
				recovered_block->data[3],
				recovered_block->data[4],
				recovered_block->data[5],
				recovered_block->data[6],
				recovered_block->data[7]
			);
			xor_blocks(recovered_block, reorder[b]);
		} else {
			if (recovered_block->id == -1) {
				printf(
					"recovering block (id = %ld, idx = %ld) from %ld blocks\n",
					b, block_array_filled, block_array_filled
				);
				recovered_block->id = b;
				reorder[b] = recovered_block;
				recovered_block->data_length_in_words = block_data_capacity_in_words; // free guess. TODO?
			} else {
				printf("found missing block %ld, but %ld was previously found?\n", b, recovered_block->id);
				exit(1);
			}
		}
	}
	printf (
		"recovered_block: id %016lx, len %016lx, flags %016lx, data:\n"
		"%016lx %016lx %016lx %016lx\n"
		"%016lx %016lx %016lx %016lx\n",
		recovered_block->id,
		recovered_block->data_length_in_words,
		recovered_block->flags,
		recovered_block->data[0],
		recovered_block->data[1],
		recovered_block->data[2],
		recovered_block->data[3],
		recovered_block->data[4],
		recovered_block->data[5],
		recovered_block->data[6],
		recovered_block->data[7]
	);
	xor_blocks(recovered_block, redundancy_block);
	printf (
		"recovered_block: id %016lx, len %016lx, flags %016lx, data:\n"
		"%016lx %016lx %016lx %016lx\n"
		"%016lx %016lx %016lx %016lx\n",
		recovered_block->id,
		recovered_block->data_length_in_words,
		recovered_block->flags,
		recovered_block->data[0],
		recovered_block->data[1],
		recovered_block->data[2],
		recovered_block->data[3],
		recovered_block->data[4],
		recovered_block->data[5],
		recovered_block->data[6],
		recovered_block->data[7]
	);
}

// returns new length of blocks and reorder (reorder will be nullptr on last entry because of redundancy block)
unsigned long make_reorder(
	const block_t ** reorder,
	block_t * blocks,
	unsigned long block_array_filled,
	unsigned long block_id_start
) {
	unsigned long max_id = 0;
	for (unsigned long b = 0; b < block_array_filled; b++) {
		if (blocks[b].id > max_id) {
			max_id = blocks[b].id;
		}
	}
	printf("max_id %ld from %ld read blocks\n", max_id, block_array_filled);
	unsigned long rel_max_id = max_id - block_id_start;
	if (rel_max_id > BLOCK_ARRAY_CAPACITY) {
		printf(
			"we have max block id %ld (section start at %ld), but only %d capacity!\n",
			max_id, block_id_start, BLOCK_ARRAY_CAPACITY
		);
	}
	unsigned long missing = rel_max_id - block_array_filled + 2;
	printf("%ld blocks missing: %ld - %ld!\n", missing, block_array_filled, rel_max_id);

	if (missing > 1) {
		printf("there are %ld blocks missing. that is beyond the error correction implemented!\n", missing);
		exit(1);
	}

	block_t * redundancy_block = 0;
	for (unsigned long b = 0; b < rel_max_id; b++) {
		reorder[b] = 0;
	}
	for (unsigned long b = 0; b < block_array_filled; b++) {
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

		reorder[id - block_id_start] = &(blocks[b]);
		printf("reorder id %ld -> idx %ld\n", id, b);
	}

	if (missing == 1) {
		if (rel_max_id == BLOCK_ARRAY_CAPACITY) {
			printf("there is no capacity for the missing block to be recovered!\n");
			exit(1);
		}
		if (!redundancy_block) {
			printf("we want to recover, but the redundancy block is not in the blocks?\n");
			exit(1);
		}
		recover_block(blocks, reorder, redundancy_block, block_array_filled);
		block_array_filled++;
	}
	return block_array_filled;
}

void write_block_data(
	FILE * output_file,
	const block_t ** blocks,
	unsigned long blocks_filled
) {
	for (unsigned long b = 0; b < blocks_filled; b++) {
		if (!blocks[b]) {
			printf("cannot write missing block %ld!\n", b);
			continue;
		}

		printf("writing block %ld: %p (%ld words) to output\n", b, blocks[b], blocks[b]->data_length_in_words);
		for (unsigned long i = 0; i < blocks[b]->data_length_in_words; i += 4) {
			printf(
				"words at %p: %016lx %016lx %016lx %016lx\n",
				&(blocks[b]->data[i]),
				blocks[b]->data[i + 0],
				blocks[b]->data[i + 1],
				blocks[b]->data[i + 2],
				blocks[b]->data[i + 3]
			);
		}
		unsigned int output_written = fwrite(
			blocks[b]->data,
			sizeof(unsigned long),
			blocks[b]->data_length_in_words,
			output_file
		);
		if (!output_written) {
			if (ferror(output_file)) {
				perror("writing block to output_file");
				exit(1);
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
	if (strcmp(input_filename, "-") != 0) {
		input_fd = open(input_filename, O_RDONLY);
		printf("input_fd: %d\n", input_fd);
		if (input_fd < 0) {
			perror(input_filename);
			exit(1);
		}
		close_input_fd_at_end = true;
	} else {
		input_fd = STDIN_FILENO;
	}

	if (strcmp(output_filename, "-") != 0) {
		output_fd = open(
			output_filename,
			O_WRONLY |
			O_CREAT,
			S_IRUSR |
			S_IWUSR |
			S_IRGRP |
			S_IRGRP |
			S_IROTH
		);
		printf("output_fd: %d\n", output_fd);
		if (output_fd < 0) {
			perror(output_filename);
			exit(1);
		}
		close_output_fd_at_end = true;
	} else {
		printf(
			"the programm will not vomit raw bytes to stdout. "
			"please provide a real filename!\n"
		);
		exit(1);
	}

	FILE * input_file = fdopen(input_fd, "r");
	FILE * output_file = fdopen(output_fd, "w");

	char * line_buffer;
	unsigned long line_buffer_filled;
	unsigned long line_number = 0;

	const unsigned long block_array_capacity = BLOCK_ARRAY_CAPACITY;
	block_t blocks [BLOCK_ARRAY_CAPACITY];
	unsigned long block_array_filled = 0;

	unsigned long * block_raw = (unsigned long *) &(blocks[0]);
	// how many words do we already have towards the next block
	unsigned long block_buffer_filled = 0;

	// maps block ids starting from reorder_block_id_start to their blocks.
	// null entries mark missing blocks
	unsigned long reorder_block_id_start;
	const block_t * reorder [BLOCK_ARRAY_CAPACITY];

	while (true) {
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
			block_raw,
			&block_buffer_filled,
			line_buffer,
			line_buffer_filled
		);

		if (block_buffer_filled == sizeof(block_t) / sizeof(unsigned long)) {
			printf(
				"filled block idx = %ld, id = %ld (@%p) with %ld words\n",
				block_array_filled, blocks[block_array_filled].id, block_raw, block_buffer_filled
			);
			block_array_filled ++;
			block_raw = (unsigned long *) &(blocks[block_array_filled]);
			block_buffer_filled = 0;
		}

		// the block we just completed
		block_t * block = &blocks[block_array_filled - 1];
		if (!(block->flags & BLOCK_REDUNDANCY)) {
			// get next block, we are not at the checking stage yet.
			continue;
		}

		// the redundancy block has the index of the first block its redundancy covers
		long block_id_start = block->id;
		block_array_filled = make_reorder(reorder, blocks, block_array_filled, block_id_start);

		printf("write blocks\n");
		write_block_data(output_file, reorder, block_array_filled - 1);
		printf("done\n");
		block_array_filled = 0;
		block_buffer_filled = 0;
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
