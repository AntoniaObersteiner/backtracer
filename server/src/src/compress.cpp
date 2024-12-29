#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <span>
#include <type_traits>
#include <vector>

#include "compress.hpp"

void assert_dictionary_capacity (const size_t dictionary_size);

extern "C" ssize_t compress_c (
	uint8_t        c_compressed [],
	size_t   const c_compressed_in_words,
	uint64_t const c_raw_data [],
	size_t   const c_raw_data_in_words,
	uint64_t       c_dictionary [],
	size_t   const c_dictionary_in_words // expected to be 256
) {
	assert_dictionary_capacity (c_dictionary_in_words);

	std::span       dictionary { c_dictionary, c_dictionary_in_words };
	std::span const raw_data   {   c_raw_data,   c_raw_data_in_words };
	std::span       compressed { c_compressed, c_compressed_in_words };
	create_dictionary(dictionary, raw_data);
	return compress(compressed, raw_data, dictionary);
}

/**
 * dictionary: fixed-length array, maps u8 compressed index -> u64 raw
 * 	all 00-entries of the dictionary are unused entries.
 * compressed:
 * 	a byte 00 means: raw is u64(0)
 * 	a byte 01 means: next 8 bytes are not compressed.
 * 	any other: use as index in dictionary
 */
static constexpr uint8_t zero_marker = 0x00;
static constexpr uint8_t  raw_marker = 0x01;
static constexpr size_t reserved = 2;

void assert_dictionary_capacity (const size_t dictionary_size) {
	if (dictionary_size != dictionary_capacity) {
		printf(
			"expected dictionary of size %ld, got %ld\n",
			dictionary_capacity, dictionary_size
		);
		exit(1);
	}
}


class dict_node {
public:
	uint64_t raw = 0;
	uint64_t occurrences = 0;

	dict_node () = default;
	dict_node (uint64_t raw) : raw(raw), occurrences(1) {}

	bool operator < (const dict_node & other) {
		return occurrences < other.occurrences;
	}
};

void create_dictionary (
	std::span<      uint64_t> const & dictionary,
	std::span<const uint64_t> const & raw_data
) {
	assert_dictionary_capacity (dictionary.size());

	std::vector<dict_node> dict_heap;
	dict_heap.reserve(dictionary.size());

	for (const auto & raw : raw_data) {
		const auto node_ptr = std::find_if(dict_heap.begin(), dict_heap.end(),
			[&raw] (const dict_node & node) {
				return node.raw == raw;
			}
		);
		if (node_ptr != dict_heap.end()) {
			node_ptr->occurrences ++;
			std::make_heap(dict_heap.begin(), dict_heap.end());
		} else {
			dict_heap.emplace_back(raw);
			std::push_heap(dict_heap.begin(), dict_heap.end());
		}

		if (dict_heap.size() >= 2 * dictionary.size()) {
			dict_heap.resize(dictionary.size());
		}
	}

	auto entry = dictionary.begin();

	for (; entry < dictionary.end(); ++entry) {
		uint8_t key = entry - dictionary.begin();
		if (key == zero_marker) {
			*entry = 0;
			continue;
		}
		if (key == raw_marker) {
			*entry = 0;
			continue;
		}
		if (dict_heap.empty()) {
			*entry = 0;
			continue;
		}

		std::pop_heap(dict_heap.begin(), dict_heap.end());
		uint64_t raw = dict_heap.back().raw;
		dict_heap.pop_back();

		*entry = raw;
	}
}

ssize_t compress (
	std::span<uint8_t>        const   compressed,
	std::span<const uint64_t> const & raw_data,
	std::span<const uint64_t> const & dictionary
) {
	auto comp = compressed.begin();

	auto push_key = [&] (std::remove_reference<decltype(dictionary)>::type::iterator const & key_as_it) {
		if (comp + 1 > compressed.end())
			return false;

		auto key = key_as_it - dictionary.begin();
		*comp = static_cast<uint8_t> (key);
		++comp;
		return true;
	};
	auto push_raw = [&] (uint64_t raw) {
		if (comp + sizeof(uint64_t) + 1 > compressed.end())
			return false;

		*comp = raw_marker;
		++comp;

		uint8_t & c = *comp;
		uint64_t & C = reinterpret_cast<uint64_t &> (c);
		C = raw;
		comp += sizeof(uint64_t);
		return true;
	};

	for (const auto & raw : raw_data) {
		auto key = std::find(dictionary.begin(), dictionary.end(), raw);
		if (key == dictionary.end()) {
			if (!push_raw(raw))
				return -1;
		} else {
			if (!push_key(key))
				return -1;
		}
	}
	return comp - compressed.begin();
}

// decompress is for C++ use only, so no
std::vector<uint64_t> decompress (
	std::span<const uint8_t>  const & compressed,
	std::span<const uint64_t> const & dictionary
) {
	std::vector<uint64_t> result;
	result.reserve(compressed.size());

	auto comp = compressed.begin();

	auto pop_key = [&] () {
		const uint8_t key = *comp;
		++comp;

		auto raw = dictionary[key];
		result.push_back(raw);
	};
	auto pop_raw = [&] () {
		++comp;

		const uint8_t & c = *comp;
		const uint64_t & C = reinterpret_cast<const uint64_t &> (c);
		uint64_t raw = C;
		result.push_back(raw);
		comp += sizeof(uint64_t);
	};
	for (; comp < compressed.end();) {
		auto key = *comp;
		if (key == zero_marker) {
			pop_key();
		} else if (key == raw_marker) {
			pop_raw();
		} else {
			pop_key();
		}
	}

	return result;
}
