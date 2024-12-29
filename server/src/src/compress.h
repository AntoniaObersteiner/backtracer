#include <stdint.h>

static const dictionary_capacity = 256;
// returns -1 if compressed is too large, then c_compressed is almost filled
ssize_t compress_c (
	uint8_t        * c_compressed,
	size_t   const   c_compressed_in_words,
	uint64_t const * c_raw_data,
	size_t   const   c_raw_data_in_words,
	uint64_t       * c_dictionary,
	size_t   const   c_dictionary_in_words // expected to be 256
);


