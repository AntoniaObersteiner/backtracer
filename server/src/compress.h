#include <stdint.h>

static const size_t dictionary_capacity = 256;
// returns -1 if compressed is too large, then c_compressed is almost filled
ssize_t compress_c (
	uint8_t        * c_compressed,
	size_t   const   c_compressed_in_words,
	uint64_t const * c_raw_data,
	size_t   const   c_raw_data_in_words,
	uint64_t       * c_dictionary,
	size_t   const   c_dictionary_in_words // expected to be 256
);

// added infront of data that is compressed (or not)
typedef struct compression_header_s {
	// if true, dictionary follows at dictionary_offset after the start of this struct
	//  then the compressed data, which needs a byte-size since its end is not word-aligned
	// if false, dictionary_length is 0 and uncompressed data starts instead of dictionary
	unsigned long is_compressed;
	unsigned long dictionary_offset;
	unsigned long dictionary_length;
	unsigned long data_length_in_bytes;
} compression_header_t;


