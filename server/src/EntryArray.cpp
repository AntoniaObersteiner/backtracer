#include "EntryArray.hpp"

RawEntryArray::RawEntryArray (const uint64_t * const buffer, const size_t length_in_words) {
	const uint64_t * current = buffer;
	while (current - buffer + 4 < length_in_words) {
		#if 1
		printf(
			"reading at offset %5ld words (%5ld bytes): type = %3ld, length = %3ld words\n",
			current - buffer, (current - buffer) * sizeof(uint64_t),
			*current, *(current + 1)
		);
		#endif
		const size_t length = reinterpret_cast<size_t>(*(current + 1));
		if (length == 0)
			break;
		self().push_back(current);
		current += length;
	}
}

EntryArray::EntryArray (const RawEntryArray & raw_entry_array) {
	for (size_t i = 0; i < raw_entry_array.size(); i++) {
		const uint64_t * const type_ptr = raw_entry_array[i];
		const size_t entry_length = reinterpret_cast<size_t>(*(type_ptr + 1));
		if (*type_ptr == BTE_INFO) {
			entry_descriptor_map.reset(new EntryDescriptorMap { raw_entry_array[i], entry_length });
			break;
		}
	}
	if (!entry_descriptor_map) {
		throw std::runtime_error(
			"there is no entry descriptor table in the btb. "
			"but the entry types are not implemented, "
			"they are read from the buffer itself."
		);
	}

	for (size_t i = 0; i < raw_entry_array.size(); i++) {
		const uint64_t * const type_ptr = raw_entry_array[i];
		const size_t entry_length = reinterpret_cast<size_t>(*(type_ptr + 1));
		#if 0
		if (i + 1 < raw_entry_array.size()) printf(
			"reading from %p, word %lx, byte %lx: typ = %lx, len = %lx "
			"to %p, word %lx, byte %lx, word diff: %lx.\n",
			type_ptr, type_ptr - raw_entry_array[0], (type_ptr - raw_entry_array[0]) * 8,
			*type_ptr, entry_length,
			raw_entry_array[i + 1], raw_entry_array[i + 1] - raw_entry_array[0],
			(raw_entry_array[i + 1] - raw_entry_array[0]) * 8,
			raw_entry_array[i + 1] - type_ptr
		); else printf(
			"reading from %p, word %lx, byte %lx: typ = %lx, len = %lx\n",
			type_ptr, type_ptr - raw_entry_array[0], (type_ptr - raw_entry_array[0]) * 8,
			*type_ptr, entry_length
		);
		fflush(stdout);
		#endif
		if (*type_ptr != BTE_INFO) {
			super().emplace_back(raw_entry_array[i], entry_length, *entry_descriptor_map);
		}
	}
}

