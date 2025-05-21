#include <iostream>
#include <format>

#include "Entry.hpp"
#include "Mapping.hpp"

Mapping::Mapping (const Entry & entry) : lifetime(Range<>::open_end(0)) {
	// read the unsigned ints of the raw data as a character array
	const std::vector<unsigned long> & payload = entry.get_payload();
	const char * name_chars = reinterpret_cast<const char *>(payload.data());
	assert_attribute_name(
		name_chars,
		payload.size() * sizeof(unsigned long),
		payload.data(),
		0,
		&is_mapping_name_char
	);
	name = std::string(name_chars);

	base = entry.attribute("mapping_base");
	task_id = entry.attribute("mapping_task_id");

	// TODO: implement dlclose entry types and respect here
	lifetime = Range<>::open_end(
		entry.attribute("tsc_time")
	);

	dbg();
}

void Mapping::dbg () const {
	std::cout
		<< " Mapping of '" << name
		<< "' base " << base
		<< ", task " << task_id
		<< ", life [" << std::hex << lifetime.start()
		<<       ", " << std::hex << lifetime.stop() << ")"
		<< std::endl;
}

std::optional<Symbol> Mapping::find_symbol (
	const SymbolTable & symbol_table,
	unsigned long virtual_address,
	unsigned long time_in_ns
) const {
	if (false) std::cout << std::format(
		"Mapping[{:30}, {:2x}, {:16x}, {:20} = {:30}]::find_symbol({:16x}, {:16x} = {:8.6} s) -- {:4s}in time -> {}",
		name,
		task_id,
		base,
		lifetime.to_string(std::hex),
		lifetime.to<double>().to_string(),
		virtual_address,
		time_in_ns,
		static_cast<double>(time_in_ns) / 1000000000.0,
		(lifetime.contains(time_in_ns) ? "" : "not "),
		(lifetime.contains(time_in_ns) ? "" : (
		symbol_table.find_symbol(virtual_address) ?
		symbol_table.find_symbol(virtual_address)->name : ""
		))
	) << std::endl;

	if (!lifetime.contains(time_in_ns))
		return std::optional<Symbol>();

	return symbol_table.find_symbol(virtual_address - base);
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
	Mapping & mapping = super().emplace_back("KERNEL", 0, task_id, Range<>::open_end(0));
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

std::string Mappings::lookup_symbol (
	unsigned long task_id,
	unsigned long virtual_address,
	unsigned long time_in_ns
) {
	std::optional<Symbol> result;
	for (const auto & binary : binaries_by_task[task_id]) {
		unsigned int mapping_index = by_task_and_binary[std::make_pair(binary, task_id)];
		const Mapping & mapping = super()[mapping_index];
		if (mapping_index >= super().size()) {
			throw std::runtime_error(
				"mapping of '" + binary + "', task " + std::to_string(task_id) + " is invalid?"
			);
		}
		SymbolTable & symbol_table = binary_symbols[binary];
		std::optional<Symbol> looked_up = mapping.find_symbol(symbol_table, virtual_address, time_in_ns);
		if (!looked_up)
			continue;

		if (result) {
			throw std::runtime_error(std::format(
				"both '{}' and '{}' map {:16x} ({} and {})",
				result->binary, binary, virtual_address,
				result->label(), looked_up->label()
			));
		}

		static_assert(std::is_copy_constructible_v<Symbol>);
		static_assert(std::is_copy_assignable_v<Symbol>);

		result = looked_up;
	}
	if (!result)
		return std::format("{:x}/{:016x}", task_id, virtual_address);

	return result->label();
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

