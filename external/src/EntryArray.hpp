#pragma once

#include <cstdint>
#include <vector>

#include "Entry.hpp"

class RawEntryArray : public std::vector<const uint64_t *> {
	using Self  = RawEntryArray;
	using Super = std::vector<const uint64_t *>;
	Self  & self  () { return *this; }
	Super & super () { return static_cast<Super &>(*this); }

public:
	const std::span<uint64_t> buffer;
	RawEntryArray (const std::span<uint64_t> buffer);
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

