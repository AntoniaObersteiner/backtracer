#include <format>

#include "EntryDescriptor.hpp"
#include "Mapping.hpp"

Entry::Entry (
	const uint64_t * const buffer,
	const size_t length_in_words,
	const EntryDescriptorMap & entry_descriptor_map
) {
	if (length_in_words < 4) {
		throw std::runtime_error(
			"cannot read an entry from a buffer with less than 4 remaining words!"
		);
	}
	entry_types entry_type = static_cast<entry_types>(*buffer);

	if (!entry_descriptor_map.contains(entry_type)) {
		throw std::runtime_error(std::format(
			"entry_type {:2x} has no attribute descriptors yet!",
			static_cast<unsigned long>(entry_type)
		));
	}

	const EntryDescriptor & entry_descriptor = entry_descriptor_map.at(entry_type);

	self()["entry_type"] = entry_type;
	size_t offset = 1;
	for (; offset < entry_descriptor.size(); offset++) {
		std::string entry_name = entry_descriptor[offset];
		self()[entry_name] = entry_descriptor.read_in(offset, buffer, length_in_words);
	}
	size_t payload_offset = offset;
	payload.resize(length_in_words - entry_descriptor.size());
	for (; offset < length_in_words; offset++) {
		payload[offset - payload_offset] = buffer[offset];
	}
	if (entry_type == BTE_MAPPING) {
		add_mapping();
	}
}

std::string Entry::to_string () const {
	std::string result;
	for (const auto & [name, value] : self()) {
		if (name == "task_id") {
			result += std::format("  {:16}: {:16x} {}\n", name, value, task_binaries(value));
		} else {
			result += std::format("  {:16}: {:16x}\n", name, value);
		}
	}
	for (size_t i = 0; i < payload.size(); i++) {
		if (self().at("entry_type") == BTE_STACK) {
			std::string symbol_name = get_symbol_name(payload[i], super().at("tsc_time"));
			result += std::format("  {:15} : {:16x} {}\n", i, payload[i], symbol_name);
		} else if (self().at("entry_type") == BTE_MAPPING) {
			const char * name = reinterpret_cast<const char *>(&payload[i]);
			result += std::format("  {:15} : {:16x} {:.8}\n", i, payload[i], name);
		} else {
			result += std::format("  {:15} : {:16x}\n", i, payload[i]);
		}
	}
	return result;
}

std::string Entry::folded (
	const Entry * previous_entry,
	bool weight_from_time
) const {
	if (self().at("entry_type") != BTE_STACK) {
		throw std::runtime_error("folded can only be called on BTE_STACK entries!");
	}

	std::string result;
	for (ssize_t i = payload.size() - 1; i >= 0; i--) {
		std::string symbol_name = get_symbol_name(payload[i], super().at("tsc_time"));
		result += symbol_name;
		if (i > 0)
			result += ";";
	}
	result += " ";
	uint64_t weight = 1;
	if (weight_from_time) {
		if (previous_entry)
			weight = self().start_time() - previous_entry->end_time();
		else
			weight = 1;
	}
	result += std::to_string(weight);
	return result;
}

std::string Entry::task_binaries (unsigned long task_id) const {
	return mappings.task_binaries(task_id);
}

void Entry::add_mapping () const {
	mappings.append(*this);
}

std::string Entry::get_symbol_name (
	unsigned long virtual_address,
	unsigned long time_in_us
) const {
	unsigned long task_id = self().at("task_id");
	return mappings.lookup_symbol(task_id, virtual_address, time_in_us);
}
