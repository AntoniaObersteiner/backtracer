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
	size_t   const c_dictionary_in_words
) {
	assert_dictionary_capacity (c_dictionary_in_words);

	std::span       dictionary { c_dictionary, c_dictionary_in_words };
	std::span const raw_data   {   c_raw_data,   c_raw_data_in_words };
	std::span       compressed { c_compressed, c_compressed_in_words };
	create_dictionary(dictionary, raw_data);
	return compress(compressed, raw_data, dictionary);
}

ssize_t compress_smart (
	uint64_t       c_dictionary_and_compressed [], // and header
	size_t   const c_dictionary_and_compressed_in_words,
	uint64_t const c_raw_data [],
	size_t   const c_raw_data_in_words,
	compression_header_t * compression_header_1
) {
	// we will try to compress into this data buffer,
	// if dictionary + compressed data don't fit, it's not worth it and we return -1.
	const unsigned long header_capacity_in_words = (sizeof(compression_header_t) - 1) / sizeof(unsigned long) + 1;
	compression_header_t * compression_header_2 = (compression_header_t *) &c_dictionary_and_compressed[0];
	uint64_t * c_dictionary = &c_dictionary_and_compressed[header_capacity_in_words];

	if (static_cast<ssize_t>(c_dictionary_and_compressed_in_words) - header_capacity_in_words <= dictionary_capacity) {
		printf("dictionary_and_compressed is too short for header and dictionary.\n");
		compression_header_1->is_compressed = false;
		compression_header_1->dictionary_length = 0;
		compression_header_1->dictionary_offset = header_capacity_in_words;
		compression_header_1->data_length_in_bytes = c_raw_data_in_words * sizeof(unsigned long);
		return -1;
	}

	std::span const raw_data   {   c_raw_data, c_raw_data_in_words };
	std::span       dictionary { c_dictionary, dictionary_capacity };
	const size_t dictionary_length = create_dictionary(dictionary, raw_data);

	uint8_t * c_compressed = reinterpret_cast<uint8_t *> (c_dictionary + dictionary_length);
	const size_t compressed_capacity_in_bytes = (
		c_dictionary_and_compressed_in_words
		- dictionary_length
		- header_capacity_in_words
	) * sizeof(unsigned long);
	std::span compressed { c_compressed, compressed_capacity_in_bytes };

	printf(
		// first part is printed in caller
		// "trying to compress\n"
		// "    btb  %16p (cap %8ld w, len %ld w)\n"
		"    into %16p (cap %8lx w =    %8lx B),\n"
		"    dict %16p (cap %8lx w =    %8lx B)\n",
		c_compressed, compressed_capacity_in_bytes / sizeof(unsigned long), compressed_capacity_in_bytes,
		c_dictionary, dictionary_length, dictionary_length * sizeof(unsigned long)
	);
	const ssize_t compressed_bytes = compress(compressed, raw_data, dictionary);
	compression_header_t * actual_compression_header;
	const uint64_t * actual_result_buffer;
	if (compressed_bytes < 0) {
		actual_result_buffer = &c_raw_data[0];
		// write the header struct it points to
		compression_header_1->is_compressed = false;
		compression_header_1->dictionary_length = 0;
		compression_header_1->dictionary_offset = header_capacity_in_words;
		compression_header_1->data_length_in_bytes = (c_raw_data_in_words - header_capacity_in_words) * sizeof(unsigned long);
		actual_compression_header = compression_header_1;
	} else {
		actual_result_buffer = &c_dictionary_and_compressed[0];
		compression_header_2->is_compressed = true;
		compression_header_2->dictionary_length = dictionary_length;
		compression_header_2->dictionary_offset = header_capacity_in_words;
		compression_header_2->data_length_in_bytes = compressed_bytes;
		actual_compression_header = compression_header_2;
	}

	size_t data_length_in_words = (actual_compression_header->data_length_in_bytes - 1) / sizeof(unsigned long) + 1;
	printf(
		"compressed: %s,\n"
		"    dict %16p (len %8lx w =    %8lx B),\n"
		"    data %16p (len %8lx w<=    %8lx B)\n",
		actual_compression_header->is_compressed ? "True" : "False",
		actual_result_buffer + actual_compression_header->dictionary_offset,
		actual_compression_header->dictionary_length,
		actual_compression_header->dictionary_length * sizeof(unsigned long),
		actual_result_buffer +
		actual_compression_header->dictionary_offset +
		actual_compression_header->dictionary_length,
		data_length_in_words,
		actual_compression_header->data_length_in_bytes
	);

	if (compressed_bytes < 0)
		return compressed_bytes;
	else
		return header_capacity_in_words + dictionary_length + data_length_in_words;
}

/**
 * dictionary: fixed-length array, maps u8 compressed index -> u64 raw
 * 	all 00-entries of the dictionary are unused entries.
 * compressed:
 * 	a byte 00 means: raw is u64(0)
 * 	a byte 01 means: next 8 bytes are not compressed.
 * 	any other: use as index in dictionary
 *
 * 	these could be changed, but currently create_dictionary relies on:
 * 	 zero_marker < raw_marker (both are noted as 0,
 * 	    compressing 0 would otherwise use the raw marker instead of the zero marker)
 * 	 raw_marker < all free keys (the used size of the dictionary returned by create_dictionary
 * 	    could not include one of the two markers, since it is set
 * 	    with the first dict key that has no dict_heap to fill it)
 */
static constexpr uint8_t zero_marker = 0x00;
static constexpr uint8_t  raw_marker = 0x01;
static constexpr size_t marker_reserved = 2;

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

size_t create_dictionary (
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

		// this cutting off of rare words causes slightly worse compression
		// but spares us a lot of realloc if raw_data is diverse
		if (dict_heap.size() >= 2 * dictionary.size()) {
			dict_heap.resize(dictionary.size());
		}
	}

	auto entry = dictionary.begin();

	size_t actual_dictionary_used = 0;

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
			if (actual_dictionary_used == 0) {
				actual_dictionary_used = entry - dictionary.begin();
			}
			*entry = 0;
			continue;
		}

		std::pop_heap(dict_heap.begin(), dict_heap.end());
		uint64_t raw = dict_heap.back().raw;
		dict_heap.pop_back();

		*entry = raw;
	}
	if (actual_dictionary_used == 0) {
		actual_dictionary_used = dictionary.size();
	}

	return actual_dictionary_used;
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
