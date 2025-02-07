#include <cstdint>
#include <string>
#include <span>

const std::span<uint64_t> mmap_file(
	const std::string & filename
);

