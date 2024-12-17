/*
 * (c) 2008-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Frank Mehnert <fm3@os.inf.tu-dresden.de>,
 *               Lukas Grützmacher <lg2@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>

#define BLOCK_SIZE 1024
#define BLOCK_DATA_SIZE (BLOCK_SIZE - 4 * sizeof(unsigned long))
const unsigned long block_data_capacity_in_bytes = BLOCK_DATA_SIZE;
const unsigned long block_data_capacity_in_words = block_data_capacity_in_bytes / sizeof(unsigned long);

enum block_flags_t {
	BLOCK_REDUNDANCY = (1UL << 0),
};

typedef struct block_t_struct {
	unsigned long id;
	unsigned long data_length_in_words;
	unsigned long flags;
	unsigned long reserved;
	unsigned long data [BLOCK_DATA_SIZE / sizeof(unsigned long)];
} block_t;
// outside of the l4re module compilation environment, we have no static assert macro
#ifdef static_assert
static_assert (
	sizeof(block_t) == BLOCK_SIZE,
	"block_t is not of the right size. please adjust block_data_capacity_in_bytes"
);
#endif

static inline
block_t make_block(
	const unsigned long * data,
	unsigned long data_length_in_words,
	unsigned long flags
) {
	static unsigned long id = 0;

	block_t block;

	block.id = id;
	block.data_length_in_words = data_length_in_words;
	block.flags = flags;
	block.reserved = 0;

	if (data_length_in_words > block_data_capacity_in_words) {
		data_length_in_words = block_data_capacity_in_words;
	}

	unsigned long i;
	for (i = 0; i < data_length_in_words; i++) {
		block.data[i] = data[i];
	}
	for (; i < block_data_capacity_in_words; i++) {
		block.data[i] = 0;
	}

	id ++;
	return block;
}

static inline
void xor_blocks(block_t * target, const block_t * to_add) {
	unsigned long min_len = target->data_length_in_words;
	if (target->data_length_in_words < min_len) {
		min_len = target->data_length_in_words;
	}
	for (unsigned long i = 0; i < min_len; i++) {
		target->data[i] = target->data[i] ^ to_add->data[i];
	}
}

// block marker[block id/word offset]
const unsigned long block_format_id_chars = 8;
const unsigned long block_format_offset_chars = 2;
const char * const block_format_default = "%s [%08lx.%02x] ";
const char * const block_marker_default = ">=<";

static inline
void print_block(
	const block_t * block,
	const char * block_marker
) {

	const unsigned word_columns = 4;
	unsigned long * raw_block = (unsigned long *) block;

	if (!block_marker)
		block_marker = block_marker_default;

	for (unsigned w = 0; w < sizeof(block_t) / sizeof(unsigned long); w++) {
		if (w % word_columns == 0) {
			printf(block_format_default, block_marker, block->id, w);
		}

		printf("%016lx ", raw_block[w]);
		if (w % word_columns == word_columns - 1)
			printf("\n");
	}

	printf("\n");
}

