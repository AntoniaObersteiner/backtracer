#include <cstdint>
#include <string>

void mmap_file(
	const std::string & filename,
	uint64_t * &buffer,
	size_t &buffer_size_in_words
);

