#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include <string>

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
);

class EntryDescriptor : public std::vector<std::string> {
	std::map<std::string, uint64_t> attribute_offsets;
	using Self = EntryDescriptor;
	Self & self () { return *this; }

public:
	EntryDescriptor (const uint64_t * const buffer, const uint64_t length_in_words);

	uint64_t read_in(
		const uint64_t offset,
		const uint64_t * const buffer,
		const uint64_t length_in_words
	) const;
};
class EntryDescriptorMap : public std::map<entry_types, EntryDescriptor> {
	using Self = EntryDescriptorMap;
	Self & self () { return *this; }

public:
	EntryDescriptorMap (const uint64_t * const buffer, const size_t length_in_words);
};

