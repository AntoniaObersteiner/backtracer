#include <fstream>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "Mapping.hpp"
#include "BinariesList.hpp"
#include "EntryArray.hpp"
#include "SymbolTable.hpp"

// TODO: eliminate code duplication with fiasco/src/jdb/jdb_btb.cpp?

void mmap_file(
	const std::string & filename,
	uint64_t * &buffer,
	size_t &buffer_size_in_words
) {
	printf("reading file '%s'\n", filename.c_str());

	struct stat stat_buffer;
	if (stat(filename.c_str(), &stat_buffer) != 0) {
		perror(filename.c_str());
		throw std::runtime_error("could not execute stat on '" + filename + "'!");
	}
	size_t buffer_size_in_bytes = stat_buffer.st_size;
	if (buffer_size_in_bytes % sizeof(unsigned long)) {
		throw std::runtime_error(
			"file '" + filename + "' is " + std::to_string(buffer_size_in_bytes)
			+ " long, which is not a whole number of words ("
			+ std::to_string(sizeof(unsigned long)) + " bytes / word)!"
		);
	}
	buffer_size_in_words = buffer_size_in_bytes / sizeof(unsigned long);

	printf(
		"file '%s' is %ld bytes, %ld words long.\n",
		filename.c_str(), buffer_size_in_bytes, buffer_size_in_words
	);

	// only needed to create mmap mapping
	int fd = open(filename.c_str(), O_RDONLY);
	void * raw_buffer = mmap(0, buffer_size_in_bytes, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	if (raw_buffer == reinterpret_cast<void *>(-1)) {
		perror("mmap");
		throw std::runtime_error("could not mmap '" + filename + "'!");
	}

	buffer = reinterpret_cast<uint64_t *>(raw_buffer);
}

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
