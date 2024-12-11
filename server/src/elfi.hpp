#include <iostream>
#include <elfio/elfio.hpp>

inline int elfi_test(std::string filename) {
	using namespace ELFIO;

	std::cerr << "reading '" << filename << "'" << std::endl;

	// Create elfio reader
	elfio reader;
	// Load ELF data
	if (!reader.load(filename)) {
		std::cout << "Can't find or process ELF file " << filename << std::endl;
		return 2;
	}
	// Print ELF file properties
	std::cout << "ELF file class : ";
	if (reader.get_class() == ELFCLASS32)
		std::cout << "ELF32" << std::endl;
	else
		std::cout << "ELF64" << std::endl;

	std::cout << "ELF file encoding : ";
	if (reader.get_encoding() == ELFDATA2LSB)
		std::cout << "Little endian" << std::endl;
	else
		std::cout << "Big endian" << std::endl;

	return 0;
}
