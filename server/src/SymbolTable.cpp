#include "SymbolTable.hpp"

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

				const Range instruction_addresses { value, size };
				const Range pages = instruction_addresses.rounded(0x1000);

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

	const SymbolPage & symbol_page = super().at(page_address);
	return symbol_page.find_symbol(instruction_pointer);
}

