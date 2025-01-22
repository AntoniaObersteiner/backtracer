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

#include "Entry.hpp"
#include "SymbolTable.hpp"

class Mapping {
public:
	std::string name;
	unsigned long base;
	unsigned long task_id;

	// might be cut off by a later mapping entry!
	Range<> lifetime;

	Mapping (
		const std::string & name,
		unsigned long base,
		unsigned long task_id,
		const Range<> & lifetime
	) : name(name),
		base(base),
		task_id(task_id),
		lifetime(lifetime)
	{
		dbg ();
	}

	void dbg () const;

	Mapping (const Entry & entry);

	std::optional<Symbol> find_symbol (
		const SymbolTable & symbol_table,
		unsigned long virtual_address,
		unsigned long time_in_ns
	) const;
};

class Mappings : public std::vector<Mapping> {
public:
	using Self = Mappings;
	using Super = std::vector<Mapping>;
	Self        & self  ()       { return *this; }
	Self  const & self  () const { return *this; }
	Super       & super ()       { return dynamic_cast<      Super &>(*this); }
	Super const & super () const { return dynamic_cast<const Super &>(*this); }

	Mappings ();
	std::map<std::pair<std::string, unsigned long>, unsigned int> by_task_and_binary;
	std::map<unsigned long, std::vector<std::string>> binaries_by_task;

	bool has_mapping (unsigned long task_id, const std::string & name) const;

	void append (const Entry & entry);

	void add_kernel_mapping (const unsigned long task_id);

	std::string task_binaries (unsigned long task_id);

	std::string lookup_symbol (
		unsigned long task_id,
		unsigned long virtual_address,
		unsigned long time_in_ns
	);

	void dbg () const;
};

extern Mappings mappings;

