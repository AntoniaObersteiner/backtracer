#include <iomanip>
#include <iostream>
#include <format>
#include <fstream>
#include <string>
#include <vector>

#include "compress.hpp"
#include "mmap_file.hpp"

template <typename T, size_t L>
void dump (std::string note, const std::span<T, L> & data) {
	std::cout << note << std::endl;

	const int values_per_line = 100 / (2 * sizeof(T) + 1);
	int already_on_line = 0;

	for (const auto & value : data) {
		std::cout << std::hex << std::setw(2 * sizeof(T)) << static_cast<int>(value);

		if (already_on_line >= values_per_line) {
			std::cout << std::endl;
			already_on_line = 0;
		} else {
			already_on_line ++;
			std::cout << " ";
		}
	}
	std::cout << std::endl;
}
template <typename Container>
void dump_wrap (std::string note, const Container & data) {
	dump(note, std::span(data));
}

void write_out(std::ofstream & output_stream, std::span<const uint64_t> data) {
	for (const auto & word : data) {
		output_stream.write(reinterpret_cast<const char *>(&word), sizeof(uint64_t));
	}
}

int main(int argc, char * argv []) {
	if (argc < 3) {
		printf(
			"usage: decompress <input> <output>\n"
			"usually, input ends in .compressed and output in .btb\n"
		);
		exit(1);
	}

	std::string input_filename { argv[1] };
	std::string output_filename { argv[2] };

	uint64_t * input_buffer;
	size_t input_size_in_words;
	mmap_file(input_filename, input_buffer, input_size_in_words);

	const uint64_t * const fixed_start = input_buffer;

	std::span input { input_buffer, input_size_in_words };

	std::ofstream output_stream {
		output_filename,
		std::ios::out
		| std::ios::trunc
		| std::ios::binary
	};

	// there are usually several (probably) compressed sections of data,
	// each with header, dictionary (if compressed) and (compressed) data
	int section_counter = 0;
	size_t remaining_input = input_size_in_words - (input_buffer - fixed_start);
	do {
		// parse the compression_header
		const size_t header_capacity_in_words = (sizeof(compression_header_t) - 1) / sizeof(uint64_t) + 1;
		if (remaining_input < header_capacity_in_words) {
			printf("input file '%s' is smaller than even the compression header??\n", input_filename.c_str());
			exit(1);
		}

		// get the following data sections (dictionary and compressed)
		compression_header_t * compression_header = reinterpret_cast<compression_header_t *> (input_buffer);
		const uint64_t * dictionary_raw = input_buffer + compression_header->dictionary_offset;
		const size_t dictionary_length_in_words = compression_header->dictionary_length;

		const uint64_t * compressed_raw = dictionary_raw + dictionary_length_in_words;
		const size_t compressed_length_in_bytes = compression_header->data_length_in_bytes;

		if (compressed_raw - fixed_start > input_size_in_words) {
			printf(
				"input file %s's section %d (dictionary or data) overflow the file end!\n",
				input_filename.c_str(), section_counter
			);
			exit(1);
		}

		const std::span dictionary { dictionary_raw, dictionary_length_in_words };
		const std::span compressed { reinterpret_cast<const uint8_t*>(compressed_raw), compressed_length_in_bytes };

		if (!compression_header->is_compressed) {
			if (compressed_length_in_bytes % sizeof(uint64_t) != 0) {
				printf("data is supposedly not compressed, but length is not multiple of word length??\n");
				exit(1);
			}

			size_t compressed_words = compressed_length_in_bytes / sizeof(uint64_t);
			printf("data was not compressed, writing %ld words to '%s'\n", compressed_words, output_filename.c_str());
			const std::span decompressed { compressed_raw, compressed_words };
			write_out(output_stream, decompressed);
			return 0;
		}

		auto decompressed = decompress(compressed, dictionary);

		printf(
			"data decompressed from %ld B, writing %ld words to '%s'\n",
			compressed_length_in_bytes, decompressed.size(), output_filename.c_str()
		);
		write_out(output_stream, std::span { decompressed });

		// a few bytes of padding to get to the next word
		// (these come from export_backtrace_buffer_section(...) rounding up the number of bytes
		// before passing to print_backtrace_buffer_section(...))
		size_t compressed_words = (compressed_length_in_bytes - 1) / sizeof(uint64_t) + 1;

		uint64_t * next_input_buffer = (
			input_buffer +
			header_capacity_in_words +
			dictionary_length_in_words +
			compressed_words
		);
		printf(
			"jumping from section\n"
			"  at %p (header: %lx w = %lx B, dict: %lx w = %lx B, data: %lx w = %lx B)\n"
			"  to %p, offset from fixed_start\n"
			"  at %p is %lx w = %lx B, remaining %lx w = %lx B, total length %lx w = %lx B\n",
			input_buffer,
			header_capacity_in_words,
			header_capacity_in_words * sizeof(uint64_t),
			dictionary_length_in_words,
			dictionary_length_in_words * sizeof(uint64_t),
			compressed_words,
			compressed_words * sizeof(uint64_t),
			next_input_buffer,
			fixed_start,
			next_input_buffer - fixed_start,
			(next_input_buffer - fixed_start) * sizeof(uint64_t),
			input_size_in_words - (next_input_buffer - fixed_start),
			(input_size_in_words - (next_input_buffer - fixed_start)) * sizeof(uint64_t),
			input_size_in_words,
			input_size_in_words * sizeof(uint64_t)
		);

		input_buffer = next_input_buffer;
		remaining_input = input_size_in_words - (input_buffer - fixed_start);
	} while (remaining_input > 0);

	printf("done.\n");
}

