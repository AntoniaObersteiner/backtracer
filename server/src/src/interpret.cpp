#include <fstream>

#include "Mapping.hpp"
#include "BinariesList.hpp"
#include "EntryArray.hpp"
#include "SymbolTable.hpp"
#include "mmap_file.hpp"

// TODO: eliminate code duplication with fiasco/src/jdb/jdb_btb.cpp?

class OutputStreams {
public:
	enum output_mode_e {
		raw,
		folded,
	};
	using cpu_id_t = uint64_t;
	const struct constructed_s {
		std::string base_name;
		std::string ending;
		output_mode_e output_mode;
	} constructed;
	const std::string   & base_name   = constructed.base_name;
	const std::string   & ending      = constructed.ending;
	const output_mode_e & output_mode = constructed.output_mode;

private:
	std::map<cpu_id_t, std::ofstream> streams;
	std::ofstream common_stream;

public:
	OutputStreams (
		const std::string & output_filename
	) : constructed(split_filename(output_filename)),
		common_stream(base_name + "." + ending)
	{}

	std::ofstream & common () {
		return common_stream;
	}

	std::ofstream & operator [] (const cpu_id_t cpu_id) {
		if (!streams.contains(cpu_id))
			streams.emplace(
				cpu_id,
				std::ofstream {
					base_name + "-" + std::to_string(cpu_id) + "." + ending,
					std::ios::out
					| std::ios::trunc
					| std::ios::binary
				}
			);

		return streams.at(cpu_id);
	}

	static struct constructed_s split_filename (
		const std::string & output_filename
	) {
		static const std::regex output_filename_regex { "(.+)\\.(interpreted|folded)" };
		std::smatch match;

		if (!std::regex_match(output_filename, match, output_filename_regex)) {
			throw std::runtime_error(
				"output file '" + output_filename + "' "
				"does not end in '.interpreted' or '.folded'!"
			);
		}

		output_mode_e output_mode;
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

		return {
			base_name,
			ending,
			output_mode
		};
	}
};

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
	OutputStreams output_streams { std::string(argv[2]) };

	std::string binaries_list_filename = "./data/binaries.list";
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
		switch (output_streams.output_mode) {
		case OutputStreams::raw: {
			std::string output = "read entry: \n" + entry.to_string();
			output_streams.common()                       << output << std::endl;
			if (entry.attribute("entry_type") == BTE_STACK) {
				output_streams[entry.attribute("cpu_id")] << output << std::endl;
			}
		}
			break;
		case OutputStreams::folded:
			if (entry.attribute("entry_type") == BTE_STACK) {
				std::string output = entry.folded(previous_entry);
				output_streams.common()            << output << std::endl;
				output_streams[entry.attribute("cpu_id")] << output << std::endl;
			}
			break;
		}
		previous_entry = &entry;
	}

	return 0;
}
