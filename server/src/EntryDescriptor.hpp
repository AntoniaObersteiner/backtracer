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

enum entry_types {
	// BTE: BackTrace Entry
	BTE_STACK    = 1 << 0,    // normal stack trace
	BTE_MAPPING  = 1 << 1,    // library mapping info
	BTE_INFO     = 1 << 2,    // information about module
	BTE_CONTROL  = 1 << 3,    // start/stop(/reset) entry
};

// a string like 'tsc_duration' that descibes an entry attribute
// will have used this many uint64_t to descibe itself
constexpr uint64_t words_per_entry_name = 2;

void assert_attribute_name (
	const char * attribute_name,
	unsigned long size_in_words = words_per_entry_name
) {
	unsigned long size_in_bytes = sizeof(uint64_t) * size_in_words;
	for (int j = 0; j < size_in_bytes; j++) {
		char c = attribute_name[j];
		if ('a' <= c && c <= 'z' || c == '_' || '0' <= c && c <= '9') {
			if (j == size_in_bytes - 1) {
				// not a '\0'?
				throw std::runtime_error("attribute_name not 0-terminated!");
			}
		} else if (c == 0) {
			if (j == 0) {
				throw std::runtime_error("attribute_name empty?");
			}
			return; // found 0-termination
		} else {
			throw std::runtime_error(std::format(
				"encountered char '{}' ({}) in attribute name??",
				c, int(c)
			));
		}
	}
}

class EntryDescriptor : public std::vector<std::string> {
	std::map<std::string, uint64_t> attribute_offsets;
	using Self = EntryDescriptor;
	Self & self () { return *this; }

public:
	EntryDescriptor (const uint64_t * const buffer, const uint64_t length_in_words) {
		self().resize(length_in_words / words_per_entry_name);
		// one attribute_name is stored in several words
		for (uint64_t index = 0; index < length_in_words / words_per_entry_name; index++) {
			uint64_t offset = index * words_per_entry_name;
			uint64_t attribute_name_numbers [words_per_entry_name + 1];
			for (uint64_t n = 0; n < words_per_entry_name; n++) {
				attribute_name_numbers[n] = buffer[offset + n];
			}
			// names must not be 0-terminated, so add that
			attribute_name_numbers[words_per_entry_name] = 0;

			const char * attribute_name = reinterpret_cast<const char *>(&attribute_name_numbers[0]);
			assert_attribute_name(attribute_name);
			std::string name { attribute_name };
			self()[index] = name;
			attribute_offsets[name] = index;
			printf("entry type has attribute %ld: '%s'\n", index, attribute_name);
		}
	}

	uint64_t read_in(
		const uint64_t offset,
		const uint64_t * const buffer,
		const uint64_t length_in_words
	) const {
		if (size() > length_in_words) {
			throw std::runtime_error(
				"reading an entry of length " + std::to_string(length_in_words)
				+ " with an EntryDescriptor for " + std::to_string(size())
				+ " words."
			);
		}
		if (offset > size()) {
			throw std::runtime_error(
				"reading offset " + std::to_string(offset)
				+ " from an EntryDescriptor for " + std::to_string(size())
				+ " words."
			);
		}
		return buffer[offset];
	}
};
class EntryDescriptorMap : public std::map<entry_types, EntryDescriptor> {
	using Self = EntryDescriptorMap;
	Self & self () { return *this; }

public:
	EntryDescriptorMap (const uint64_t * const buffer, const size_t length_in_words) {
		// TODO: use meta-entry for types...
		// ignore length of entry in buffer+1, that's in length_in_words already,
		// ignore tsc stuff in +2 and +3
		// ignore version in +4

		uint64_t type_count = *(buffer + 5);
		const uint64_t * entry_descriptor_lengths       = buffer + 6;
		const uint64_t * current_entry_descriptor_names = buffer + 6 + type_count;

		for (uint64_t i = 0; i < type_count; i++) {
			if (current_entry_descriptor_names - buffer > length_in_words)
				throw std::runtime_error ("entries descriptor names continue after entry??");

			entry_types entry_type = static_cast<entry_types>(1 << i);
			size_t entry_descriptor_length = entry_descriptor_lengths[i];
			if (entry_descriptor_length == 0) {
				// some entry types are not yet implemented.
				// thus, they don't have descriptors yet.
				continue;
			}

			self().emplace(entry_type, EntryDescriptor (
				current_entry_descriptor_names, entry_descriptor_length * words_per_entry_name
			));
			current_entry_descriptor_names += entry_descriptor_length * words_per_entry_name;
		}
	}
};

