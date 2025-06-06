#include "EntryArray.hpp"
#include "rethrow_error.hpp"

RawEntryArray::RawEntryArray (const std::span<uint64_t> buffer) : buffer(buffer) {
	const uint64_t * previous = buffer.data();
	const uint64_t * current = buffer.data();
	while (true) {
		auto offset_in_words = [&] () {
			return current - buffer.data();
		};
		if (offset_in_words() == buffer.size()) {
			return;
		} else if (offset_in_words() > buffer.size()) {
			throw std::runtime_error(std::format(
				"the end of the file is not reached exactly, "
				"from entry @{}, offset {}. \n"
				"overshoots end by {} words.",
				reinterpret_cast<const void *>(previous),
				previous - buffer.data(),
				offset_in_words() - buffer.size()
			));
		} else if (offset_in_words() + 4 >= buffer.size()) {
			throw std::runtime_error(
				"the end of the file is not reached exactly, "
				"there are " + std::to_string(buffer.size() - offset_in_words()) +
				" words remaining."
			);
		}
		#if 0
		printf(
			"reading at offset %5ld words (%5ld bytes) of length %5ld words: type = %3ld, length = %3ld words\n",
			offset_in_words(), offset_in_words() * sizeof(uint64_t), buffer.size(),
			*current, *(current + 1)
		);
		#endif

		const size_t length = reinterpret_cast<size_t>(*(current + 1));
		if (length == 0)
			break;

		try {
			self().push_back(current);
		} catch (std::exception & e) {
			throw rethrow_error<std::runtime_error>(e, std::format(
				"there was an error in raw entry number {} @{},\nfirst bytes {:016x} {:016x} {:016x} {:016x}.",
				self().size(),
				reinterpret_cast<const void*>(current),
				*current,
				*(current + 1),
				*(current + 2),
				*(current + 3)
			));
		}
		previous = current;
		current += length;
	}
}

EntryArray::EntryArray (const RawEntryArray & raw_entry_array) {
	for (size_t i = 0; i < raw_entry_array.size(); i++) {
		const uint64_t * const type_ptr = raw_entry_array[i];
		const size_t entry_length = reinterpret_cast<size_t>(*(type_ptr + 1));
		if (*type_ptr == BTE_INFO) {
			try {
				entry_descriptor_map.reset(new EntryDescriptorMap { raw_entry_array[i], entry_length });
			} catch (std::exception & e) {
				throw rethrow_error<std::runtime_error>(e, std::format(
					"there was an error in entry number {} @{},\nfirst bytes {:016x} {:016x} {:016x} {:016x}.",
					i,
					reinterpret_cast<const void *>(type_ptr),
					*type_ptr,
					*(type_ptr + 1),
					*(type_ptr + 2),
					*(type_ptr + 3)
				));
			}
			break;
		}
		switch (*type_ptr) {
		case BTE_STACK:   [[fallthrough]]
		case BTE_MAPPING: [[fallthrough]]
		case BTE_CONTROL: [[fallthrough]]
		case BTE_STATS:
			continue;
		default:
			throw std::runtime_error(std::format(
				"the entry at {}, {:x} words behind buffer start @{},\n"
				"has entry type {:x}, which is not recognized by this program.",
				reinterpret_cast<const void *>(type_ptr),
				type_ptr - raw_entry_array.buffer.data(),
				reinterpret_cast<const void *>(raw_entry_array.buffer.data()),
				*type_ptr
			));
		}
	}
	if (!entry_descriptor_map) {
		throw std::runtime_error(
			"there is no entry descriptor table in the btb. "
			"but the entry types are not implemented, \n"
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
		try {
			super().emplace_back(raw_entry_array[i], entry_length, *entry_descriptor_map);
		} catch (std::exception & e) {
			throw rethrow_error<std::runtime_error>(e, std::format(
				"there was an error in entry number {},\nfirst bytes {:016x} {:016x} {:016x} {:016x}.",
				i, *type_ptr,
				*(type_ptr + 1),
				*(type_ptr + 2),
				*(type_ptr + 3)
			));
		}
	}

	std::sort(super().begin(), super().end(), [](const Entry & a, const Entry & b) {
		return a.attribute("tsc_time") < b.attribute("tsc_time");
	});
}

