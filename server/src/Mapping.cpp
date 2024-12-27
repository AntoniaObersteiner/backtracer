#include <iostream>
#include <format>

#include "elfi.hpp"
#include "Entry.hpp"
#include "Mapping.hpp"

Mapping::Mapping (const Entry & entry) {
	// read the unsigned ints of the raw data as a character array
	const std::vector<unsigned long> & payload = entry.get_payload();
	const char * name_chars = reinterpret_cast<const char *>(payload.data());
	assert_attribute_name(name_chars, payload.size() * sizeof(unsigned long));
	name = std::string(name_chars);

	base = entry.at("mapping_base");
	task_id = entry.at("mapping_task_id");

	std::cout
		<< " Mapping of '" << name
		<< "' base " << base
		<< ", task " << task_id
		<< std::endl;
}

std::string Mapping::lookup_symbol (ELFIO::elfio & reader, unsigned long virtual_address) const {
	if (false) std::cout
		<< "Mapping[" << name
		<< ", "<< task_id << "]::lookup_symbol("
		<< std::hex << virtual_address << ")"
		<< std::endl;
	return get_symbol(name, reader, virtual_address - base);
}

Mappings::Mappings () {
	add_kernel_mapping(1);
}

bool Mappings::has_mapping (unsigned long task_id, const std::string & name) const {
	const std::vector<std::string> & binaries = binaries_by_task.at(task_id);
	return std::find(binaries.begin(), binaries.end(), name) != binaries.end();
}

void Mappings::append (const Entry & entry) {
	super().emplace_back(entry);
	Mapping & mapping = self()[super().size() - 1];
	by_task_and_binary[std::make_pair(mapping.name, mapping.task_id)] = super().size() - 1;
	binaries_by_task[mapping.task_id].emplace_back(mapping.name);

	if (!has_mapping(mapping.task_id, "KERNEL")) {
		add_kernel_mapping(mapping.task_id);
	}
}

void Mappings::add_kernel_mapping (const unsigned long task_id) {
	super().emplace_back("KERNEL", 0, task_id);
	Mapping & mapping = super()[super().size() - 1];
	by_task_and_binary[std::make_pair(mapping.name, mapping.task_id)] = super().size() - 1;
	binaries_by_task[mapping.task_id].emplace_back(mapping.name);
}

std::string Mappings::task_binaries (unsigned long task_id) {
	if (!binaries_by_task.contains(task_id)) {
		return "<task " + std::to_string(task_id) + " has no binaries>";
	}
	std::vector<std::string> & binaries = binaries_by_task.at(task_id);
	std::string result = "";
	for (int i = 0; i < binaries.size() - 1; i++) {
		result += binaries[i] + ", ";
	}
	if (binaries.size()) {
		result += binaries.back();
	} else {
		result = "<empty>";
	}
	return result;
}

std::string Mappings::lookup_symbol (unsigned long task_id, unsigned long virtual_address) {
	std::string result = "";
	std::string result_binary = "";
	for (const auto & binary : binaries_by_task[task_id]) {
		unsigned int mapping_index = by_task_and_binary[std::make_pair(binary, task_id)];
		const Mapping & mapping = super()[mapping_index];
		if (mapping_index >= super().size()) {
			throw std::runtime_error(
				"mapping of '" + binary + "', task " + std::to_string(task_id) + " is invalid?"
			);
		}
		ELFIO::elfio & reader = elfio_readers[binary];
		std::string looked_up = mapping.lookup_symbol(reader, virtual_address);
		if (looked_up.empty())
			continue;

		if (!result.empty()) {
			throw std::runtime_error(std::format(
				"both '{}' and '{}' map {:16x} ({} and {})",
				result_binary, binary, virtual_address,
				result, looked_up
			));
		}

		result = looked_up;
		result_binary = binary;
	}
	return result_binary + "/" + result;
}

void Mappings::dbg () const {
	std::cout << std::format(
		"[{:2}] {:20} {:16} {:8} {:16} [{:2}] {:8} {:8} {:16}",
		"mp", "name", "            base", " task_id", "         address",
		"tb", " task_id", "  binary", "         address"
	) << std::endl;

	for (int i = 0; i < super().size(); i++) {
		const Mapping & mapping = super()[i];
		std::cout << std::format(
			"[{:2}] {:20} {:16x} {:8x} {:16x}",
			i, mapping.name, mapping.base, mapping.task_id,
			reinterpret_cast<unsigned long>(const_cast<Mapping *>(&mapping))
		);
		if (i < by_task_and_binary.size()) {
			auto it = by_task_and_binary.begin();
			for (int _ = 0; _ < i; _++)
				it++;
			const auto [key, mapping_index] = *it;
			const Mapping & mapping_by = super()[mapping_index];
			const auto [binary, task_id] = key;
			std::cout << std::format(
				"  [{:2}] {:8x} {:8} {:16x}",
				i, task_id, binary,
				reinterpret_cast<unsigned long>(const_cast<Mapping *>(&mapping_by))
			);
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

