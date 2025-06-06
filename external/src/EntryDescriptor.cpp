#include <iostream>
#include <vector>
#include <format>

#include "EntryDescriptor.hpp"

bool is_attribute_name_char (char c) {
	return (
		c == '_'
		|| 'a' <= c && c <= 'z'
		|| '0' <= c && c <= '9'
	);
}
bool is_mapping_name_char (char c) {
	return (
		c == '_'
		|| 'a' <= c && c <= 'z'
		|| '0' <= c && c <= '9'
		|| c == '.'
		|| c == '/'
		|| c == '-'
		|| c == '+'
	);
}

void assert_attribute_name (
	const char * attribute_name,
	unsigned long size_in_words,
	const uint64_t * buffer,
	const unsigned long offset,
	bool (* char_check) (char)
) {
	unsigned long size_in_bytes = sizeof(uint64_t) * size_in_words;
	for (int j = 0; j < size_in_bytes; j++) {
		char c = attribute_name[j];
		if (char_check(c)) {
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
				"encountered char '{}' ({}) as byte {} of {} in attribute name,\n"
				"from buffer    @{}, offset {}, first word {:016x}.\n"
				"attribute_name @{}, '{}'.",
				c, int(c),
				j, size_in_bytes,
				reinterpret_cast<const void*>(buffer), offset,
				*buffer,
				reinterpret_cast<const void*>(attribute_name),
				attribute_name
			));
		}
	}
}

EntryDescriptor::EntryDescriptor (const uint64_t * const buffer, const uint64_t length_in_words) {
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
		assert_attribute_name(attribute_name, words_per_entry_name, buffer, offset);
		std::string name { attribute_name };
		self()[index] = name;
		attribute_offsets[name] = index;
		if (false) printf("entry type has attribute %ld: '%s'\n", index, attribute_name);
	}
}

uint64_t EntryDescriptor::read_in(
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
EntryDescriptorMap::EntryDescriptorMap (const uint64_t * const buffer, const size_t length_in_words) {
	// TODO: use meta-entry for types...
	// ignore length of entry in buffer+1, that's in length_in_words already,
	// ignore tsc stuff in +2 and +3
	// ignore version in +4

	// TODO: remove this code for "version 2", that was just for one trace output and can be discarded now!
	uint64_t version = *(buffer + 4);
	size_t padding = 0;
	if (version == 2)
		padding = 1;

	uint64_t type_count = *(buffer + 5);
	const uint64_t * entry_descriptor_lengths       = buffer + 6 + padding;
	const uint64_t * current_entry_descriptor_names = buffer + 6 + padding + type_count;

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

