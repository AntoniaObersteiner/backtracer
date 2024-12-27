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

#include "elfi.hpp"
#include "Entry.hpp"

class Mapping {
public:
	std::string name;
	unsigned long base;
	unsigned long task_id;

	Mapping (
		const std::string & name,
		unsigned long base,
		unsigned long task_id
	) : name(name),
		base(base),
		task_id(task_id)
	{
		std::cout
			<< "Mapping of '" << name
			<< "' base " << base
			<< ", task " << task_id
			<< std::endl;
	}

	Mapping (const Entry & entry);

	std::string lookup_symbol (ELFIO::elfio & reader, unsigned long virtual_address) const;
};

class Mappings : public std::vector<Mapping> {
public:
	using Self = Mappings;
	using Super = std::vector<Mapping>;
	Self  & self  () { return *this; }
	Super & super () { return dynamic_cast<Super &>(*this); }
	const Super & super () const { return dynamic_cast<const Super &>(*this); }

	Mappings ();
	std::map<std::pair<std::string, unsigned long>, unsigned int> by_task_and_binary;
	std::map<unsigned long, std::vector<std::string>> binaries_by_task;

	bool has_mapping (unsigned long task_id, const std::string & name) const;

	void append (const Entry & entry);

	void add_kernel_mapping (const unsigned long task_id);

	std::string task_binaries (unsigned long task_id);

	std::string lookup_symbol (unsigned long task_id, unsigned long virtual_address);

	void dbg () const;
};

extern std::map<std::string, ELFIO::elfio> elfio_readers;
extern Mappings mappings;

