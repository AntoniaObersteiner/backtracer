#include <iostream>
#include <format>
#include <string>
#include <vector>

#include "compress.hpp"

template <typename T, size_t L>
void dump (std::string note, const std::span<T, L> & data) {
	std::cout << note << std::endl;

	const int values_per_line = 64 / sizeof(T);
	int already_on_line = 0;

	for (const auto & value : data) {
		std::cout << std::format("{:8x}", value);

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

int main() {
	std::vector<uint64_t> raw_data;
	raw_data.resize(1024);
	for (auto i = 0; i < raw_data.size(); ++i) {
		if (i % 8 < 4)
			raw_data[i] = i % 8;
		else
			raw_data[i] = i;
	}
	std::vector<uint8_t> compressed;
	compressed.reserve(raw_data.size() * sizeof(uint64_t) * 2);

	std::array<uint64_t, dictionary_capacity> dictionary;
	dump_wrap("raw_data:", raw_data);

	create_dictionary(dictionary, raw_data);

	dump_wrap("dict:", dictionary);

	ssize_t compressed_size = compress(compressed, raw_data, dictionary);

	if (compressed_size == -1) {
		std::cout << "could not compress into result array!" << std::endl;
	}

	dump_wrap("compressed:", compressed);
}
