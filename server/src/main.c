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
#include <sys/mman.h>
#include <l4/sys/kip.h>
#include <l4/sys/debugger.h>
#include <l4/re/env.h>
#include <l4/sys/linkage.h>

unsigned long print_utcb(
	const char * prefix,
	l4_utcb_t * utcb,
	l4_msgtag_t tag
) {
	unsigned long words = l4_msgtag_words(tag);
	printf(
		"%s l4_msgtag (%ld, %d, %d, %d) \n",
		prefix,
		l4_msgtag_label(tag),
		l4_msgtag_words(tag),
		l4_msgtag_items(tag),
		l4_msgtag_flags(tag)
	);

  printf("%s [", prefix);
	for (unsigned i = 0; i < words; i++) {
		printf("%d: %ld%s", i, l4_utcb_mr_u(utcb)->mr[i], (i < words - 1) ? ", " : "");
	}
  printf("]\n");

	return words;
}

l4_msgtag_t
l4_debugger_get_backtrace_buffer_section(
	l4_cap_idx_t cap,
	unsigned long * buffer,
	unsigned long buffer_capacity_in_bytes,
	unsigned long * returned_words,
	unsigned long * remaining_words
) L4_NOTHROW {
	l4_utcb_t * utcb = l4_utcb();
	l4_utcb_mr_u(utcb)->mr[0] = L4_DEBUGGER_GET_BTB_SECTION;
	l4_utcb_mr_u(utcb)->mr[1] = buffer;
	l4_utcb_mr_u(utcb)->mr[2] = buffer_capacity_in_bytes;
	l4_msgtag_t tag = l4_msgtag(0, 3, 0, 0);

	print_utcb("=>>", utcb, tag);

	l4_msgtag_t syscall_result = l4_invoke_debugger(cap, tag, utcb);

	print_utcb("<<=", utcb, syscall_result);

	if (l4_msgtag_has_error(syscall_result)) {
		*returned_words = 0;
		*remaining_words = 0;
		return syscall_result;
	}

	*returned_words  = l4_utcb_mr_u(utcb)->mr[0];
	*remaining_words = l4_utcb_mr_u(utcb)->mr[1];

	return syscall_result;
}

void print_backtrace_buffer_section (unsigned long * buffer, unsigned long words) {
	const unsigned word_width = 4;
	for (unsigned w = 0; w < words; w++) {
		if (w % word_width == 0)
			printf("--> %2d: ", w);

		printf("%16lx ", buffer[w]);
		if (w % word_width == word_width - 1)
			printf("\n");
	}

	printf("\n");
}

bool export_backtrace_buffer_section (l4_cap_idx_t cap) {
	const unsigned kumem_page_order = 3;
	const unsigned kumem_capacity_in_bytes = (1 << (kumem_page_order + 10));
	static l4_addr_t kumem = 0;
	if (!kumem) {
		// TODO: this allocation is never freed. this makes sense,
		// because it basically has life-of-the-process lifetime.
		if (l4re_util_kumem_alloc(&kumem, kumem_page_order, L4_BASE_TASK_CAP, l4re_env()->rm)) {
			printf("!!! could not allocate %d kiB kumem!!!\n", 1 << kumem_page_order);
		}
		printf("successfully allocated %d kiB kumem at %p\n", 1 << kumem_page_order, (void *) kumem);
	}

	unsigned long * buffer = (unsigned long *) kumem;
	unsigned long returned_words;
	unsigned long remaining_words;
	l4_msgtag_t syscall_result = l4_debugger_get_backtrace_buffer_section(
		cap,
		buffer,
		kumem_capacity_in_bytes,
		&returned_words,
		&remaining_words
	);

	print_backtrace_buffer_section(buffer, returned_words);

	return (remaining_words > 0);
}

int main(void) {
	l4_cap_idx_t dbg_cap = L4_BASE_DEBUGGER_CAP;
	bool is_valid = l4_is_valid_cap(dbg_cap) > 0;
	printf(">>> dbg_cap %ld is %svalid <<<\n", dbg_cap, is_valid ? "" : "not ");

	// TODO: talk to the other thread, e.g. via a semaphore

	while (export_backtrace_buffer_section(dbg_cap)) {}
}
