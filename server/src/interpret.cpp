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

#include "elfi.hpp"

// TODO: eliminate code duplication with fiasco/src/jdb/jdb_btb.cpp?

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

void assert_attribute_name (const char * attribute_name, unsigned long size_in_words = 1, bool assert_null_terminated = false) {
	unsigned long string_length = sizeof(uint64_t) * words_per_entry_name * size_in_words;
	for (int j = 0; j < string_length; j++) {
		char c = attribute_name[j];
		if ('a' <= c && c <= 'z' || c == '_' || '0' <= c && c <= '9') {
			if (assert_null_terminated && j == string_length - 1) {
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
		if (entry_type == BTE_MAPPING) {
			add_mapping();
		}
		size_t payload_offset = offset;
		payload.resize(length_in_words - entry_descriptor.size());
		for (; offset < length_in_words; offset++) {
			payload[offset - payload_offset] = buffer[offset];
		}
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
			} else {
				result += std::format("  {:15} : {:16x}\n", i, payload[i]);
			}
		}
		return result;
	}
};

class Mapping {
public:
	const Entry & entry;
	std::string name;
	unsigned long base;
	unsigned long size;
	unsigned long task_id;

	Mapping (const Entry & entry) : entry(entry) {
		// write these integers into the array as they are in the raw data
		unsigned long name_array[5] = {
			entry.at("mapping_name1"),
			entry.at("mapping_name2"),
			entry.at("mapping_name3"),
			entry.at("mapping_name4"),
			0 // assert_attribute_name falsely assumes my char * is 0-terminated.
			// actually, this is not guaranteed by the buffer protocol.
		};
		// read them as a character array
		char * name_chars = reinterpret_cast<char *>(&(name_array[0]));
		assert_attribute_name(name_chars, 4 * sizeof(unsigned long));
		name = std::string(name_chars);

		base = entry.at("mapping_base");
		size = entry.at("mapping_size");
		task_id = entry.at("mapping_task_id");

		std::cout
			<< "Mapping of '" << name
			<< "' base " << base
			<< ", size " << size
			<< ", task " << task_id
			<< std::endl;
	}

	std::string lookup_symbol (ELFIO::elfio & reader, unsigned long virtual_address) {
		return get_symbol(reader, virtual_address - base);
	}
};

std::map<std::string, ELFIO::elfio> elfio_readers;

class Mappings : public std::vector<Mapping> {
public:
	using Self = Mappings;
	using Super = std::vector<Mapping>;
	Self  & self  () { return *this; }
	Super & super () { return dynamic_cast<Super &>(*this); }

	Mappings () {}
	std::map<std::pair<std::string, unsigned long>, Mapping *> by_task_and_binary;
	std::map<unsigned long, std::vector<std::string>> binaries_by_task;

	void emplace_back (const Entry & entry) {
		super().emplace_back(entry);
		Mapping & mapping = self().back();
		by_task_and_binary[std::make_pair(mapping.name, mapping.task_id)] = &mapping;
		binaries_by_task[mapping.task_id].emplace_back(mapping.name);
	}

	std::string task_binaries (unsigned long task_id) {
		if (!binaries_by_task.contains(task_id)) {
			return "<task " + std::to_string(task_id) + " has no binaries>";
		}
		std::vector<std::string> & binaries = binaries_by_task.at(task_id);
		std::string result = "";
		for (int i = 0; i < binaries.size() - 1; i++) {
			result += binaries[i] + ", ";
		}
		if (binaries.size()) {
			result += binaries.back();
		} else {
			result = "<empty>";
		}
		return result;
	}

	std::string lookup_symbol (unsigned long task_id, unsigned long virtual_address) {
		std::string result = "";
		std::string result_binary = "";
		for (const auto & binary : binaries_by_task[task_id]) {
			Mapping * mapping = by_task_and_binary[std::make_pair(binary, task_id)];
			if (!mapping) {
				throw std::runtime_error(
					"mapping of '" + binary + "', task " + std::to_string(task_id) + " is invalid?"
				);
			}
			ELFIO::elfio & reader = elfio_readers[binary];
			std::string looked_up = mapping->lookup_symbol(reader, virtual_address);
			if (looked_up.empty())
				continue;

			if (!result.empty()) {
				throw std::runtime_error(std::format(
					"both '{}' and '{}' map {:16x} ({} and {})",
					result_binary, binary, virtual_address,
					result, looked_up
				));
			}

			result = looked_up;
			result_binary = binary;
		}
		return result_binary + "/" + result;
	}
};

static Mappings mappings;

inline std::string Entry::task_binaries (unsigned long task_id) const {
	return mappings.task_binaries(task_id);
}

inline void Entry::add_mapping () const {
	mappings.emplace_back(*this);
}

inline std::string Entry::get_symbol_name (unsigned long virtual_address) const {
	unsigned long task_id = self().at("task_id");
	return mappings.lookup_symbol(task_id, virtual_address);
}

class RawEntryArray : public std::vector<const uint64_t *> {
	using Self = RawEntryArray;
	Self & self () { return *this; }

public:
	RawEntryArray (const uint64_t * const buffer, const size_t length_in_words) {
		const uint64_t * current = buffer;
		while (current - buffer + 4 < length_in_words) {
			const size_t length = reinterpret_cast<size_t>(*(current + 1));
			if (length == 0)
				break;
			self().push_back(current);
			current += length;
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
			#if 0
			if (i + 1 < raw_entry_array.size()) printf(
				"reading from %p, word %lx, byte %lx: typ = %lx, len = %lx "
				"to %p, word %lx, byte %lx, word diff: %lx.\n",
				type_ptr, type_ptr - raw_entry_array[0], (type_ptr - raw_entry_array[0]) * 8,
				*type_ptr, entry_length,
				raw_entry_array[i + 1], raw_entry_array[i + 1] - raw_entry_array[0],
				(raw_entry_array[i + 1] - raw_entry_array[0]) * 8,
				raw_entry_array[i + 1] - type_ptr
			); else printf(
				"reading from %p, word %lx, byte %lx: typ = %lx, len = %lx\n",
				type_ptr, type_ptr - raw_entry_array[0], (type_ptr - raw_entry_array[0]) * 8,
				*type_ptr, entry_length
			);
			#endif
			fflush(stdout);
			if (*type_ptr != BTE_INFO) {
				self().emplace_back(raw_entry_array[i], entry_length, *entry_descriptor_map);
			}
		}
	}
};

#define BUFFER_CAPACITY_IN_WORDS (1 << 16)

class BinariesList : public std::map<std::string, std::string> {
public:
	using Self = BinariesList;
	Self & self () { return *this; }
	static const std::regex line_regex;

	BinariesList (std::string filename = "./binaries.list") {
		std::ifstream file (filename);
		if (!file.is_open()) {
			throw std::runtime_error("couldn't open '" + filename + "'!");
		}

		constexpr size_t line_capacity = 1 << 12 - 1;
		char line [line_capacity + 1];

		while (true) {
			file.getline(&line[0], line_capacity + 1);
			size_t bytes_read = file.gcount();
			if (bytes_read <= 0) {
				if (file.eof()) {
					std::cout << "file '" << filename << "' read exhaustively." << std::endl;
					break;
				} else {
					throw std::runtime_error("file '" + filename + "' can't be read from!");
				}
			}

			// empty lines are fine
			if (bytes_read == 1)
				continue;

			// string match result
			std::cmatch match;
			bool matched = std::regex_match(line,  match, line_regex);
			if (!matched) {
				throw std::runtime_error(
					"line '" + std::string(line) + "' is not or the format 'label: filepath'"
				);
			}

			std::string name = match[1];
			std::string path = match[2];
			if (self().contains(name) && self().at(name) != path) {
				throw std::runtime_error(
					"for binary '" + name + "', "
					"there already is path '" + self().at(name) + "' "
					"and now also '" + path + "'?"
				);
			}

			self()[name] = path;
			std::cout << "binary '" << name << "' at path '" << path << "'" << std::endl;
		}
	}
};
const std::regex BinariesList::line_regex { "([^:]+): *([^ ]+)" };

void mmap_file(
	const std::string & filename,
	uint64_t * &buffer,
	size_t &buffer_size_in_words
) {
	printf("reading file '%s'\n", filename.c_str());

	struct stat stat_buffer;
	if (stat(filename.c_str(), &stat_buffer) != 0) {
		perror(filename.c_str());
		throw std::runtime_error("could not execute stat on '" + filename + "'!");
	}
	size_t buffer_size_in_bytes = stat_buffer.st_size;
	if (buffer_size_in_bytes % sizeof(unsigned long)) {
		throw std::runtime_error(
			"file '" + filename + "' is " + std::to_string(buffer_size_in_bytes)
			+ " long, which is not a whole number of words ("
			+ std::to_string(sizeof(unsigned long)) + " bytes / word)!"
		);
	}
	buffer_size_in_words = buffer_size_in_bytes / sizeof(unsigned long);

	printf(
		"file '%s' is %ld bytes, %ld words long.",
		filename.c_str(), buffer_size_in_bytes, buffer_size_in_words
	);

	// only needed to create mmap mapping
	int fd = open(filename.c_str(), O_RDONLY);
	void * raw_buffer = mmap(0, buffer_size_in_bytes, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	if (raw_buffer == reinterpret_cast<void *>(-1)) {
		perror("mmap");
		throw std::runtime_error("could not mmap '" + filename + "'!");
	}

	buffer = reinterpret_cast<uint64_t *>(raw_buffer);
}

int main(int argc, char * argv []) {
	// TODO: use argp / similar

	std::string binaries_list_filename = "./binaries.list";
	BinariesList binaries_list { binaries_list_filename };

	for (const auto &[name, path] : binaries_list) {
		elfio_readers[name] = get_elfio_reader(path);
		// elfi_test(path);
	}

	std::cout << std::endl;

	if (argc < 2) {
		throw std::runtime_error("missing args: need input file (.btb)");
	}

	std::string filename { argv[1] };
	uint64_t * buffer;
	size_t buffer_size_in_words;
	mmap_file(filename, buffer, buffer_size_in_words);

	EntryArray entry_array(RawEntryArray (buffer, buffer_size_in_words));

	for (const auto & entry : entry_array) {
		printf("read entry: \n%s\n", entry.to_string().c_str());
	}

	return 0;
}
