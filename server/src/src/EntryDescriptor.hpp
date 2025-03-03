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
	BTE_STATS    = 1 << 4,    // histogram of time by stack depth
};
static constexpr size_t entry_type_count = 5;
static constexpr std::string entry_type_names [entry_type_count] = {
	"BTE_STACK",
	"BTE_MAPPING",
	"BTE_INFO",
	"BTE_CONTROL",
	"BTE_STATS" ,
};
static inline constexpr const std::string & entry_type_name (const entry_types type) {
	for (size_t i = 0; i < entry_type_count; i++)
		if (static_cast<entry_types>(1 << i) == type)
			return entry_type_names[i];
	throw std::out_of_range(std::format(
		"no entry_types value for raw '{}'.",
		static_cast<uint64_t>(type)
	));
}

// a string like 'tsc_duration' that descibes an entry attribute
// will have used this many uint64_t to descibe itself
constexpr uint64_t words_per_entry_name = 2;

void assert_attribute_name (
	const char * attribute_name,
	unsigned long size_in_words,
	const uint64_t * buffer,
	const unsigned long offset
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

