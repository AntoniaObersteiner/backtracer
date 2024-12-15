#include <iostream>
#include <format>
#include <elfio/elfio.hpp>
#include <elfio/elfio_dump.hpp>

inline const char * get_section_type_name (ELFIO::Elf_Word section_type) {
	using namespace ELFIO;
	const char * section_type_name = 0;

	for (
		int stti = 0;
		stti < sizeof(section_type_table) / sizeof(struct section_type_table_t);
		stti++
	) {
		if (section_type_table[stti].key == section_type) {
			section_type_name = section_type_table[stti].str;
			break;
		}
	}
	if (section_type_name == 0) {
		throw std::runtime_error(
			"no section_type_name for type " + std::to_string(section_type)
		);
	}
	return section_type_name;
}
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
	std::cout << "ELF file class: ";
	if (reader.get_class() == ELFCLASS32)
		std::cout << "ELF32" << std::endl;
	else
		std::cout << "ELF64" << std::endl;

	std::cout << "ELF file encoding: ";
	if (reader.get_encoding() == ELFDATA2LSB)
		std::cout << "Little endian" << std::endl;
	else
		std::cout << "Big endian" << std::endl;


	// Print ELF file sections info
	Elf_Half sec_num = reader.sections.size();
	std::cout << "Number of sections: " << sec_num << std::endl;
	std::cout << " [sc] label               size type" << std::endl;
	for (int i = 0; i < sec_num; ++i) {
		section* psec = reader.sections[i];
		std::cout << std::format(
			" [{:2}] {:15} {:8} {:20}",
			i, psec->get_name(),
			psec->get_size(),
			get_section_type_name(psec->get_type())
		) << std::endl;
		// Access section's data
		const char* p = reader.sections[i]->get_data();

		if (psec->get_type() == SHT_SYMTAB) {
			const symbol_section_accessor symbols(reader, psec);
			std::cout << std::format(
				"   [{:2}] {:30} {:16} {:16} {} {} {:16} {}",
				"sb", "name", "value", "size", "bind", "type", "section_index", "other"
			) << std::endl;
			for (unsigned int j = 0; j < symbols.get_symbols_num(); ++j) {
				std::string name;
				Elf64_Addr value;
				Elf_Xword size;
				unsigned char bind;
				unsigned char type;
				Elf_Half section_index;
				unsigned char other;
				symbols.get_symbol(
					j, name, value, size, bind,
					type, section_index, other
				);
				std::cout << std::format(
					"   [{:2}] {:30} {:16x} {:16x} {} {} {:16x} {}",
					j, name, value, size, bind, type, section_index, other
				) << std::endl;
			}
		}
	}

	// Print ELF file segments info
	Elf_Half seg_num = reader.segments.size();
	std::cout << "Number of segments: " << seg_num << std::endl;
	std::cout << " [sg]            flags  virtual address     size in file   size in memory" << std::endl;
	for (int i = 0; i < seg_num; ++i) {
		const segment* pseg = reader.segments[i];
		std::cout << std::format(
			" [{:2}] {:16x} {:16x} {:16x} {:16x}",
			i, pseg->get_flags(),
			pseg->get_virtual_address(),
			pseg->get_file_size(),
			pseg->get_memory_size()
		) << std::endl;
		// Access segments's data
		const char* p = reader.segments[i]->get_data();
	}

	return 0;
}
