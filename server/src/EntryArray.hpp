#pragma once

#include <cstdint>
#include <vector>

#include "Entry.hpp"

class RawEntryArray : public std::vector<const uint64_t *> {
	using Self = RawEntryArray;
	Self & self () { return *this; }

public:
	RawEntryArray (const uint64_t * const buffer, const size_t length_in_words);
};

class EntryArray : public std::vector<Entry> {
	using Self  = EntryArray;
	using Super = std::vector<Entry>;
	Self  & self  () { return *this; }
	Super & super () { return static_cast<Super &>(*this); }
	std::unique_ptr<EntryDescriptorMap> entry_descriptor_map;

public:
	EntryArray (const RawEntryArray & raw_entry_array);
};

