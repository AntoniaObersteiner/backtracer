#pragma once
#include <elfio/elfio.hpp>
#include <elfio/elfio_dump.hpp>

const char * get_section_type_name (ELFIO::Elf_Word section_type);

ELFIO::elfio get_elfio_reader (const std::string& filename);

std::string demangle (const std::string & mangled);
const std::string get_symbol (
	const std::string & binary_name,
	ELFIO::elfio & reader,
	unsigned long instruction_pointer
);

int elfi_test(std::string filename);
