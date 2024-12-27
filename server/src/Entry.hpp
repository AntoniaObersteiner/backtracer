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
	) {
		if (length_in_words < 4) {
			throw std::runtime_error(
				"cannot read an entry from a buffer with less than 4 remaining words!"
			);
		}
		entry_types entry_type = static_cast<entry_types>(*buffer);

		if (!entry_descriptor_map.contains(entry_type)) {
			throw std::runtime_error(std::format(
				"entry_type {:2x} has no attribute descriptors yet!",
				static_cast<unsigned long>(entry_type)
			));
		}

		const EntryDescriptor & entry_descriptor = entry_descriptor_map.at(entry_type);

		self()["entry_type"] = entry_type;
		size_t offset = 1;
		for (; offset < entry_descriptor.size(); offset++) {
			std::string entry_name = entry_descriptor[offset];
			self()[entry_name] = entry_descriptor.read_in(offset, buffer, length_in_words);
		}
		size_t payload_offset = offset;
		payload.resize(length_in_words - entry_descriptor.size());
		for (; offset < length_in_words; offset++) {
			payload[offset - payload_offset] = buffer[offset];
		}
		if (entry_type == BTE_MAPPING) {
			add_mapping();
		}
	}

	const std::vector<unsigned long> & get_payload () const {
		return payload;
	}

	void add_mapping () const;
	std::string get_symbol_name (unsigned long virtual_address) const;
	// return the binaries loaded by task with given id
	std::string task_binaries (unsigned long task_id) const;

	std::string to_string () const {
		std::string result;
		for (const auto & [name, value] : self()) {
			if (name == "task_id") {
				result += std::format("  {:16}: {:16x} {}\n", name, value, task_binaries(value));
			} else {
				result += std::format("  {:16}: {:16x}\n", name, value);
			}
		}
		for (size_t i = 0; i < payload.size(); i++) {
			if (self().at("entry_type") == BTE_STACK) {
				std::string symbol_name = get_symbol_name(payload[i]);
				result += std::format("  {:15} : {:16x} {}\n", i, payload[i], symbol_name);
			} else if (self().at("entry_type") == BTE_MAPPING) {
				const char * name = reinterpret_cast<const char *>(&payload[i]);
				result += std::format("  {:15} : {:16x} {:.8}\n", i, payload[i], name);
			} else {
				result += std::format("  {:15} : {:16x}\n", i, payload[i]);
			}
		}
		return result;
	}
};

#include "Mapping.hpp"
inline std::string Entry::task_binaries (unsigned long task_id) const {
	return mappings.task_binaries(task_id);
}

inline void Entry::add_mapping () const {
	mappings.append(*this);
}

inline std::string Entry::get_symbol_name (unsigned long virtual_address) const {
	unsigned long task_id = self().at("task_id");
	return mappings.lookup_symbol(task_id, virtual_address);
}
