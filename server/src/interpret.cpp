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

Mappings mappings;
std::map<std::string, SymbolTable> binary_symbols;

int main(int argc, char * argv []) {
	// TODO: use argp / similar

	std::string binaries_list_filename = "./binaries.list";
	BinariesList binaries_list { binaries_list_filename };

	for (const auto &[name, path] : binaries_list) {
		binary_symbols.emplace(name, SymbolTable(name, get_elfio_reader(path)));
	}

	std::cout << std::endl;

	if (argc < 2) {
		throw std::runtime_error("missing args: need input file (.btb)");
	}

	std::string filename { argv[1] };
	uint64_t * buffer;
	size_t buffer_size_in_words;
	mmap_file(filename, buffer, buffer_size_in_words);
	printf("buffer: %p\n", buffer);

	EntryArray entry_array(RawEntryArray (buffer, buffer_size_in_words));

	for (const auto & entry : entry_array) {
		printf("read entry: \n%s\n", entry.to_string().c_str());
	}

	return 0;
}
