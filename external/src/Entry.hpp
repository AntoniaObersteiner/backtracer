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
	using Super = std::map<std::string, uint64_t>;

	Self        & self  ()       { return *this; }
	Self  const & self  () const { return *this; }
	Super       & super ()       { return dynamic_cast<      Super &>(*this); }
	Super const & super () const { return dynamic_cast<const Super &>(*this); }

	// must be a contiguous memory type
	std::vector<uint64_t> payload;

public:
	uint64_t const * entry_buffer; // pointer to the raw data, there is no safeguard that it isn't freed
	uint64_t buffer_offset;

	Entry (
		const uint64_t * const entry_buffer,
		const std::span<const uint64_t> & complete_buffer,
		const size_t length_in_words,
		const EntryDescriptorMap & entry_descriptor_map
	);

	const std::vector<unsigned long> & get_payload () const {
		return payload;
	}

	const unsigned long start_time_ns () const {
		return attribute("tsc_time");
	}
	const unsigned long end_time_ns () const {
		return attribute("tsc_time") + attribute("tsc_duration");
	}

	const bool has_attribute(const std::string & attribute_key) const {
		if (super().contains(attribute_key))
			return true;

		// return this one early, because the rest of the code uses it
		if (attribute_key == "entry_type")
			return false;

		// because of the special status of BTE_INFO, cpu_id could not easily be added to it.
		// but we can just assume it's cpu 0, shouldn't really matter
		if (attribute("entry_type") == BTE_INFO && attribute_key == "cpu_id")
			return true;

		return false;
	}

	const uint64_t attribute(const std::string & attribute_key) const {
		if (super().contains(attribute_key))
			return super().at(attribute_key);

		// catch these separately, because the rest of the code uses them
		if (attribute_key == "entry_type" || attribute_key == "tsc_time")
			throw std::out_of_range(std::format(
				"access to field '{}' is invalid.",
				attribute_key
			));

		// because of the special status of BTE_INFO, cpu_id could not easily be added to it.
		// but we can just assume it's cpu 0, shouldn't really matter
		if (attribute("entry_type") == BTE_INFO && attribute_key == "cpu_id")
			return 0;

		throw std::out_of_range(std::format(
			"access to field '{}' in entry of type '{}' ({}) with timestamp '{}' is invalid.",
			attribute_key,
			entry_type_name(static_cast<entry_types>(super().at("entry_type"))),
			super().at("entry_type"),
			super().at("tsc_time")
		));
	}

	void add_mapping () const;
	std::string get_symbol_name (
		unsigned long virtual_address,
		unsigned long time_in_ns
	) const;
	// return the binaries loaded by task with given id
	std::string task_binaries (unsigned long task_id) const;

	std::string to_string () const;
	std::string to_hex_string () const;
	std::string folded (const Entry * previous_entry, bool with_cpu_id, bool weight_from_time = true) const;
};

