#pragma once
#include <iostream>
#include <cstdint>
#include <vector>
#include <map>
#include <fstream>
#include <memory>
#include <format>
#include <regex>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "EntryDescriptor.hpp"

class Entry : public std::map<std::string, uint64_t> {
	using Self = Entry;
	Self & self () { return *this; }
	const Self & self () const { return *this; }
	// must be a contiguous memory type
	std::vector<uint64_t> payload;

public:
	Entry (
		const uint64_t * const buffer,
		const size_t length_in_words,
		const EntryDescriptorMap & entry_descriptor_map
	);

	const std::vector<unsigned long> & get_payload () const {
		return payload;
	}

	void add_mapping () const;
	std::string get_symbol_name (unsigned long virtual_address) const;
	// return the binaries loaded by task with given id
	std::string task_binaries (unsigned long task_id) const;

	std::string to_string () const;
};

