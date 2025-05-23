#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include "map_with_errors.hpp"

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
template<>
struct std::formatter<entry_types> : std::formatter<int>, std::formatter<std::string> {
    // use format specifiers for int
    using std::formatter<int>::parse;

    auto format(entry_types const & entry_type, auto & ctx) const {
        auto out = ctx.out();
        out = std::format_to(out, "[");

        ctx.advance_to(out);
        out = std::formatter<int>::format(static_cast<int>(entry_type), ctx);
        out = std::format_to(out, " ");
        ctx.advance_to(out);
        out = std::formatter<std::string>::format(entry_type_name(entry_type), ctx);

        return std::format_to(out, "]");
    }
};

// a string like 'tsc_duration' that descibes an entry attribute
// will have used this many uint64_t to descibe itself
constexpr uint64_t words_per_entry_name = 2;

bool is_attribute_name_char(char c);
bool is_mapping_name_char(char c);
void assert_attribute_name (
	const char * attribute_name,
	unsigned long size_in_words,
	const uint64_t * buffer,
	const unsigned long offset,
	bool (* char_check) (char) = &is_attribute_name_char
);

class EntryDescriptor : public std::vector<std::string> {
	map_with_errors<std::string, uint64_t> attribute_offsets;
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
class EntryDescriptorMap : public map_with_errors<entry_types, EntryDescriptor> {
	using Self = EntryDescriptorMap;
	Self & self () { return *this; }

public:
	EntryDescriptorMap (const uint64_t * const buffer, const size_t length_in_words);
};

