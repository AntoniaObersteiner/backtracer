#include <iostream>
#include <cstdint>
#include <vector>
#include <map>
#include <fstream>
#include <memory>
#include <format>

// TODO: eliminate code duplication with fiasco/src/jdb/jdb_btb.cpp?

enum entry_types {
	// BTE: BackTrace Entry
	BTE_STACK    = 1 << 0,    // normal stack trace
	BTE_MAPPING  = 1 << 1,    // library mapping info
	BTE_INFO     = 1 << 2,    // information about module
	BTE_START    = 1 << 3,    // start entry
	BTE_STOP     = 1 << 4,    // stop entry
};

// a string like 'tsc_duration' that descibes an entry attribute
// will have used this many uint64_t to descibe itself
constexpr uint64_t words_per_entry_name = 2;

void assert_attribute_name (const char * attribute_name) {
	for (int j = 0; j < sizeof(uint64_t) * words_per_entry_name; j++) {
		char c = attribute_name[j];
		if ('a' <= c && c <= 'z' || c == '_' || '0' <= c && c <= '9') {
			if (j == 15) {
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
			const char * attribute_name = reinterpret_cast<const char *>(buffer + offset);
			// they should be 0-terminated (and 0-padded)
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

class Entry : public std::map<std::string, uint64_t> {
	using Self = Entry;
	Self & self () { return *this; }
	const Self & self () const { return *this; }
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
			throw std::runtime_error (
				"entry_type " + std::to_string(entry_type)
				+ " has no attribute descriptors yet!\n"
			);
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
		printf("read entry: \n%s\n", to_string().c_str());
	}

	std::string to_string() const {
		std::string result;
		for (const auto & [name, value] : self()) {
			result += std::format("  {:16}: {:16x}\n", name, value);
		}
		for (size_t i = 0; i < payload.size(); i++) {
			result += std::format("  {:15} : {:16x}\n", i, payload[i]);
		}
		return result;
	}
};

class RawEntryArray : public std::vector<const uint64_t *> {
	using Self = RawEntryArray;
	Self & self () { return *this; }

public:
	RawEntryArray (const uint64_t * const buffer, const size_t length_in_words) {
		const uint64_t * current = buffer;
		while (current - buffer + 4 < length_in_words) {
			const size_t length = reinterpret_cast<size_t>(*(current + 1));
			self().push_back(current);
			current += length;
			if (length == 0)
				break;
		}
	}
};

class EntryArray : public std::vector<Entry> {
	using Self = EntryArray;
	Self & self () { return *this; }
	std::unique_ptr<EntryDescriptorMap> entry_descriptor_map;

public:
	EntryArray (const RawEntryArray & raw_entry_array) {
		for (size_t i = 0; i < raw_entry_array.size(); i++) {
			const uint64_t * const type_ptr = raw_entry_array[i];
			const size_t entry_length = reinterpret_cast<size_t>(*(type_ptr + 1));
			if (*type_ptr == BTE_INFO) {
				entry_descriptor_map.reset(new EntryDescriptorMap { raw_entry_array[i], entry_length });
				break;
			}
		}
		if (!entry_descriptor_map) {
			throw std::runtime_error(
				"there is no entry descriptor table in the btb. "
				"but the entry types are not implemented, "
				"they are read from the buffer itself."
			);
		}

		for (size_t i = 0; i < raw_entry_array.size(); i++) {
			const uint64_t * const type_ptr = raw_entry_array[i];
			const size_t entry_length = reinterpret_cast<size_t>(*(type_ptr + 1));
			if (*type_ptr != BTE_INFO) {
				self().emplace_back(raw_entry_array[i], entry_length, *entry_descriptor_map);
			}
		}
	}
};

#define BUFFER_CAPACITY_IN_WORDS (1 << 16)

int main(int argc, char * argv []) {
	// TODO: use argp / similar

	uint64_t buffer[BUFFER_CAPACITY_IN_WORDS];

	if (argc < 2) {
		throw std::runtime_error("missing args: need input file (.btb)");
	}

	std::string filename { argv[1] };
	printf("reading file '%s'\n", filename.c_str());
	std::ifstream file (filename, std::ios::binary);
	if (!file.is_open()) {
		throw std::runtime_error("failed opening '" + filename + "'!");
	}

	char * read_alias = reinterpret_cast<char *>(&buffer[0]);
	size_t capacity = BUFFER_CAPACITY_IN_WORDS * sizeof(uint64_t);

	file.read(read_alias, capacity);
	if (file.eof()) {
		printf("file read exhaustively!\n");
	} else {
		if (!file) {
			throw std::runtime_error("failed reading from '" + filename + "'!");
		} else {
			printf("file is longer than %ld bytes. that is not yet implemented!.\n", capacity);
		}
	}
	size_t length_in_words = file.gcount();

	printf("read %ld words from file '%s'\n", length_in_words, filename.c_str());

	EntryArray entry_array(RawEntryArray (buffer, length_in_words));

	return 0;
}
