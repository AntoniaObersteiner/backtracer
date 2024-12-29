#include <iomanip>
#include <iostream>
#include <format>
#include <string>
#include <vector>

#include "compress.hpp"

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
std::vector<uint64_t> get_raw_data(
	const size_t size,
	const double mixing_factor
) {
	std::vector<uint64_t> raw_data;
	raw_data.reserve(size);
	uint64_t mod = 64;
	const int threshold = static_cast<int>(mixing_factor * static_cast<double>(mod));

	for (auto i = 0; i < size; ++i) {
		uint64_t value = i;
		if (i % mod > threshold)
			value = i % 8;
		raw_data.push_back(value);
	}
	return raw_data;
}

double run_experiment(double mixing_factor) {
	bool do_dumps = false;
	auto raw_data = get_raw_data(1 << 12, mixing_factor);

	std::vector<uint8_t> compressed;
	compressed.resize(raw_data.size() * sizeof(uint64_t) * 2);

	std::array<uint64_t, dictionary_capacity> dictionary;
	if (do_dumps) dump_wrap("raw_data:", raw_data);

	create_dictionary(dictionary, raw_data);
	if (do_dumps) dump_wrap("dictionary:", dictionary);

	ssize_t compressed_size = compress(compressed, raw_data, dictionary);

	if (compressed_size < 0) {
		std::cout << "could not compress into result array!" << std::endl;
		exit(1);
	}

	std::span compressed_cut { compressed.data(), static_cast<size_t>(compressed_size) };

	size_t raw_size = raw_data.size() * sizeof(decltype(raw_data)::value_type);
	size_t comp_size = compressed_cut.size();
	size_t dict_size = dictionary.size() * sizeof(decltype(dictionary)::value_type);
	double ratio = 100.0 * static_cast<double>(comp_size + dict_size) / static_cast<double>(raw_size);
	std::cout << std::format(
		"mixing_factor {:1.3f}: compressed {:5} B into {:5} B + {} B dict, ratio: {:7.3f} % {}",
		mixing_factor, raw_size, comp_size, dict_size, ratio, std::string(static_cast<size_t>(.3 * ratio), '#')
	) << std::endl;

	if (do_dumps) dump_wrap("compressed:", compressed_cut);

	auto decompressed = decompress(compressed_cut, dictionary);

	if (do_dumps) dump_wrap("decompressed:", decompressed);

	if (raw_data.size() != decompressed.size()) {
		std::cout << "raw_data and decompressed differ in length, "
			"therefore cannot be equal" << std::endl;
		return 1;
	}
	auto [raw_mismatch, decomp_mismatch] = std::mismatch(raw_data.begin(), raw_data.end(), decompressed.begin());
	if (raw_mismatch != raw_data.end()) {
		int index = raw_mismatch - raw_data.begin();
		std::cout
			<< "raw_data and decompressed differ at " << index
			<< ", raw_data: " << raw_data[index]
			<< ", decompressed: " << decompressed[index]
			<< std::endl;
	} else {
		if (do_dumps) std::cout << "no difference found, decompression works!" << std::endl;
	}

	return ratio;
}

int main() {
	for (double mixing_factor = 0; mixing_factor <= 1; mixing_factor += .1) {
		run_experiment(mixing_factor);
	}
}
