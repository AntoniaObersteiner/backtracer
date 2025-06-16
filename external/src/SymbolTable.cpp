#include "SymbolTable.hpp"
#include <format>

bool Symbol::starts_in_page(const size_t page_address) const {
	return instruction_addresses.rounded(0x1000).start() == page_address;
}

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
				insert_symbol(Symbol{name, binary, instruction_addresses});
			}
		}
	}
}
void SymbolTable::insert_symbol (
	const Symbol & symbol
) {
	const Range pages = symbol.instruction_addresses.rounded(0x1000);

	for (const auto & page : pages) {
		super()[page].push_back(symbol);
	}
}
SymbolTable::SymbolTable (const std::string & symbol_table_filename) {
	std::ifstream file { symbol_table_filename };
	std::array<char, 4096> buf;
	while (!file.eof()) {
		file.getline(buf.data(), buf.size() - 1);
		std::string line { buf.data() };
		if (line.size())
			insert_symbol(Symbol::from_file_line(line));
	}
}

void SymbolTable::export_to_file (const std::filesystem::path & symbol_table_filename) const {
	std::filesystem::create_directories(symbol_table_filename.parent_path());
	std::ofstream file { symbol_table_filename };
	for (const auto & [page_address, page] : super()) {
		for (const auto & symbol : page) {
			if (!symbol.starts_in_page(page_address))
				continue;

			file << symbol.to_file_line() << "\n";
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

