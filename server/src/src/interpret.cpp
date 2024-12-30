#include <fstream>

#include "Mapping.hpp"
#include "BinariesList.hpp"
#include "EntryArray.hpp"
#include "SymbolTable.hpp"
#include "mmap_file.hpp"

// TODO: eliminate code duplication with fiasco/src/jdb/jdb_btb.cpp?

enum output_mode_e {
	raw,
	folded,
};

std::ofstream open_output_file (
	const std::string & output_filename,
	output_mode_e & output_mode
) {
	static const std::regex output_filename_regex { "(.+)\\.(interpreted|folded)" };
	std::smatch match;

	if (!std::regex_match(output_filename, match, output_filename_regex)) {
		throw std::runtime_error(
			"output file '" + output_filename + "' "
			"does not end in '.interpreted' or '.folded'!"
		);
	}

	std::string base_name = match[1].str();
	std::string ending = match[2].str();

	if (ending == "interpreted") {
		output_mode = raw;
	} else if (ending == "folded") {
		output_mode = folded;
	} else {
		throw std::logic_error(
			"output file '" + output_filename + "' "
			"does not end in '.interpreted' or '.folded'!"
		);
	}

	std::ofstream output_stream {
		output_filename,
		std::ios::out
		| std::ios::trunc
		| std::ios::binary
	};

	return output_stream;
}

Mappings mappings;
std::map<std::string, SymbolTable> binary_symbols;

int main(int argc, char * argv []) {
	// TODO: use argp / similar

	if (argc < 2) {
		throw std::runtime_error("missing args: need input file (.btb)");
	}
	std::string tracebuffer_filename { argv[1] };

	if (argc < 3) {
		throw std::runtime_error("missing args: needs output file (.interpreted/.folded)");
	}
	output_mode_e output_mode;
	auto output_stream = open_output_file(std::string(argv[2]), output_mode);


	std::string binaries_list_filename = "./binaries.list";
	BinariesList binaries_list { binaries_list_filename };

	for (const auto &[name, path] : binaries_list) {
		binary_symbols.emplace(name, SymbolTable(name, get_elfio_reader(path)));
	}

	std::cout << std::endl;

	uint64_t * buffer;
	size_t buffer_size_in_words;
	mmap_file(tracebuffer_filename, buffer, buffer_size_in_words);
	printf("buffer: %p\n", buffer);

	EntryArray entry_array(RawEntryArray (buffer, buffer_size_in_words));

	const Entry * previous_entry = nullptr;

	for (const auto & entry : entry_array) {
		switch (output_mode) {
		case raw:
			output_stream
				<< "read entry:" << std::endl
				<< entry.to_string() << std::endl;
			break;
		case folded:
			if (entry.at("entry_type") == BTE_STACK)
				output_stream << entry.folded(previous_entry) << std::endl;
			break;
		}
		previous_entry = &entry;
	}

	return 0;
}
