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
#include <stdbool.h>
#include <sys/mman.h>
#include <l4/sys/debugger.h>
#include <l4/re/env.h>
#include <l4/sys/irq.h>

#include "block.h"
#include "btb_syscall.h"

int main(void) {
	l4_cap_idx_t dbg_cap = L4_BASE_DEBUGGER_CAP;
	bool is_valid = l4_is_valid_cap(dbg_cap) > 0;
	printf(">>> dbg_cap %ld is %svalid <<<\n", dbg_cap, is_valid ? "" : "not ");

	// TODO: wait for some event? nope, do than in post-processing.

	// 977 * 2^10 us ~= 1000 ms
	unsigned long mantissa = 977;
	unsigned long exponent = 10;
	unsigned long timeout_us = mantissa * (1 << exponent);
	l4_timeout_t timeout = l4_ipc_timeout(
		mantissa, exponent,
		mantissa, exponent
	);

	for (int i = 0; i < 10; i++) {
		// TODO: we probably want to wait for a complete page, but not busily
		// except that, at the end, we want to get any remainder out.

		unsigned long remaining_words = export_backtrace_buffer_section(dbg_cap, true);

		if (remaining_words == 0) {
			printf(
				"wait for results with timeout %ld us == %f ms...\n",
				timeout_us, (double) timeout_us / 1000.0
			);
			// TODO: replace with waiting on BTB-IRQ-cap that we get from dbg_cap.
			// TODO: alternatively: l4_thread_switch(other thread among traced)
			// pseudo-sleep
			l4_msgtag_t sleep_result = l4_irq_receive(dbg_cap, timeout);
			if (l4_msgtag_has_error(sleep_result)) {
				printf("sleep error!\n");
			}
		}
	}
	// export remaining non-full section
	unsigned long remaining_words = export_backtrace_buffer_section(dbg_cap, false);

	if (remaining_words)
		printf(
			"!!! there is still remaining %ld data in the kernel-side backtrace buffer!\n",
			remaining_words
		);
}
