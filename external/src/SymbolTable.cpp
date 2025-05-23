#include "SymbolTable.hpp"
#include <format>

SymbolTable::SymbolTable (
	const std::string & binary,
	const ELFIO::elfio & reader
) : binary(binary) {
	using namespace ELFIO;

	// ELF file sections info
	Elf_Half sec_num = reader.sections.size();
	for (int i = 0; i < sec_num; ++i) {
		section* psec = reader.sections[i];

		// Access section's data
		const char* p = reader.sections[i]->get_data();

		if (psec->get_type() == SHT_SYMTAB) {
			const symbol_section_accessor symbols(reader, psec);
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

				// symbols with length 0 are currently not used by our consumers
				// despite the facte that e.g. interrupt handlers of fiasco have length 0
				// and maybe should get a default length so they can be shown
				if (size == 0)
					continue;

				const Range instruction_addresses { value, size };
				const Range pages = instruction_addresses.rounded(0x1000);

				if (false) std::cout << std::format("{:30}: {:30} = {:30} has {}",
					binary,
					instruction_addresses.to_string(std::hex),
					pages.to_string(std::hex),
					name
				) << std::endl;
				for (const auto & page : pages) {
					super()[page].emplace_back(
						name, binary,
						instruction_addresses
					);
				}
			}
		}
	}
}

std::optional<Symbol> SymbolTable::find_symbol (const uint64_t instruction_pointer) const {
	uint64_t page_address = (instruction_pointer / 0x1000) * 0x1000;
	if (!super().contains(page_address))
		return std::optional<Symbol> ();

	if (false) std::cout << std::format("paddr {:16x}", page_address) << std::endl;

	const SymbolPage & symbol_page = super().at(page_address);
	return symbol_page.find_symbol(instruction_pointer);
}

