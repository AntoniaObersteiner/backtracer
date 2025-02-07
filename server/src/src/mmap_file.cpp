#include <unistd.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdexcept>
#include "mmap_file.hpp"

const std::span<uint64_t> mmap_file(
	const std::string & filename
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
	size_t buffer_size_in_words = buffer_size_in_bytes / sizeof(unsigned long);

	printf(
		"file '%s' is %ld bytes, %ld words long.\n",
		filename.c_str(), buffer_size_in_bytes, buffer_size_in_words
	);

	// empty file cannot be mmap'ed
	if (buffer_size_in_bytes == 0) {
		return std::span<uint64_t> {};
	}

	// only needed to create mmap mapping
	int fd = open(filename.c_str(), O_RDONLY);
	if (fd < 0) {
		perror("open");
		throw std::runtime_error("could not open '" + filename + "'!");
	}

	void * raw_buffer = mmap(0, buffer_size_in_bytes, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);

	if (raw_buffer == reinterpret_cast<void *>(-1)) {
		perror("mmap");
		throw std::runtime_error("could not mmap '" + filename + "'!");
	}

	return std::span<uint64_t> {
		reinterpret_cast<uint64_t *>(raw_buffer),
		buffer_size_in_words
	};
}

