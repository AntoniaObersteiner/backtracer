#include <cstdint>
#include <span>

static constexpr size_t dictionary_capacity = 256;
// returns -1 if compressed is too large, then c_compressed is almost filled
extern "C" ssize_t compress_c (
	uint8_t        * c_compressed,
	size_t   const   c_compressed_in_words,
	uint64_t const * c_raw_data,
	size_t   const   c_raw_data_in_words,
	uint64_t       * c_dictionary,
	size_t   const   c_dictionary_in_words // expected to be 256
);

void create_dictionary (
	std::span<      uint64_t> const & dictionary,
	std::span<const uint64_t> const & raw_data
);

ssize_t compress (
	std::span<uint8_t>        const   compressed,
	std::span<const uint64_t> const & raw_data,
	std::span<const uint64_t> const & dictionary
);

std::vector<uint64_t> decompress (
	std::span<const uint8_t>  const & compressed,
	std::span<const uint64_t> const & dictionary
);
