#include <fstream>
#include <filesystem>

#include "Mapping.hpp"
#include "BinariesList.hpp"
#include "EntryArray.hpp"
#include "SymbolTable.hpp"
#include "mmap_file.hpp"
#include "rethrow_error.hpp"

using namespace std::literals;

// TODO: eliminate code duplication with fiasco/src/jdb/jdb_btb.cpp?

class OutputStreams {
public:
	enum output_mode_e {
		raw,
		btb_lines,
		folded,
		histogram,
		durations,
	};
	constexpr static size_t output_mode_count = 5;
	constexpr static std::string output_mode_endings [output_mode_count] = {
		"interpreted",
		"btb_lines",
		"folded",
		"histogram",
		"durations",
	};
	static_assert(output_mode_endings[raw]       == "interpreted");
	static_assert(output_mode_endings[btb_lines] == "btb_lines");
	static_assert(output_mode_endings[folded]    == "folded");
	static_assert(output_mode_endings[histogram] == "histogram");
	static_assert(output_mode_endings[durations] == "durations");
	constexpr static std::string output_mode_endings_joined (const std::string sep) {
		std::string result = "";
		for (size_t i = 0; i < output_mode_count; i++) {
			if (i > 0)
				result += sep;
			result += output_mode_endings[i];
		}
		return result;
	}

	using cpu_id_t = uint64_t;

	const struct constructed_s {
		std::string base_name;
		std::string ending;
		output_mode_e output_mode;
	} constructed;
	const std::string   & base_name   = constructed.base_name;
	const std::string   & ending      = constructed.ending;
	const output_mode_e & output_mode = constructed.output_mode;

private:
	std::map<cpu_id_t, std::ofstream> streams;
	std::ofstream common_stream;
	bool do_multi_processor;

public:
	OutputStreams (
		const std::filesystem::path & output_filename,
		const bool do_multi_processor
	) : constructed(split_filename(output_filename)),
		common_stream(base_name + "." + ending),
		do_multi_processor(do_multi_processor)
	{}

	std::ofstream & common () {
		return common_stream;
	}

	bool does_multi_processor () const {
		return do_multi_processor;
	}

	std::ofstream & operator [] (const cpu_id_t cpu_id) {
		if (!streams.contains(cpu_id))
			streams.emplace(
				cpu_id,
				std::ofstream {
					base_name + "-" + std::to_string(cpu_id) + "." + ending,
					std::ios::out
					| std::ios::trunc
					| std::ios::binary
				}
			);

		return streams.at(cpu_id);
	}

	void line (const std::string & text, uint64_t cpu_id, bool also_to_multi_processor_stream = true) {
		common() << text << std::endl;
		if (do_multi_processor && also_to_multi_processor_stream)
			(*this)[cpu_id] << text << std::endl;
	}

	static struct constructed_s split_filename (
		const std::string & output_filename
	) {
		static const std::string output_filename_regex_string { "(.+)\\.(" + output_mode_endings_joined("|") + ")" };
		static const std::regex output_filename_regex { output_filename_regex_string };
		std::smatch match;

		if (!std::regex_match(output_filename, match, output_filename_regex)) {
			throw std::runtime_error(
				"output filename '"s + output_filename + "' "
				"is not matched by '"s + output_filename_regex_string + "'!"
			);
		}

		output_mode_e output_mode;
		std::string base_name = match[1].str();
		std::string ending = match[2].str();

		size_t output_mode_index;
		for (output_mode_index = 0; output_mode_index < output_mode_count; output_mode_index++) {
			if (ending == output_mode_endings[output_mode_index]) {
				output_mode = static_cast<output_mode_e>(output_mode_index);
				break;
			}
		}
		if (output_mode_index == output_mode_count) {
			throw std::logic_error(
				"output file '" + output_filename + "' with ending '" + ending + "' "
				"does not end in '." + output_mode_endings_joined("' or '.") + "'!"
			);
		}

		return {
			base_name,
			ending,
			output_mode
		};
	}
};

Mappings mappings;
map_with_errors<std::string, SymbolTable> binary_symbols;

void interpret(
	const std::span<uint64_t> buffer,
	const BinariesList & binaries_list,
	OutputStreams & output_streams
) {
	EntryArray entry_array { RawEntryArray { buffer } };

	std::cerr << "successfully read raw data" << std::endl;

	const Entry * previous_entry = nullptr;

	for (const auto & entry : entry_array) {
		switch (output_streams.output_mode) {
		case OutputStreams::raw: {
			std::string output = "read entry: \n" + entry.to_string();
			output_streams.line(output, entry.attribute("cpu_id"), entry.attribute("entry_type") == BTE_STACK);
		}
			break;
		case OutputStreams::btb_lines: {
			std::string output = std::format("btb @{:16x}: {}", entry.buffer_offset, entry.to_hex_string());
			output_streams.line(output, entry.attribute("cpu_id"), entry.attribute("entry_type") == BTE_STACK);
		}
			break;
		case OutputStreams::folded:
			if (entry.attribute("entry_type") == BTE_STACK) {
				std::string output = entry.folded(previous_entry, output_streams.does_multi_processor());
				output_streams.line(output, entry.attribute("cpu_id"));
			}
			break;
		case OutputStreams::histogram:
			if (entry.attribute("entry_type") == BTE_STATS) {
				static size_t hist_counter = 0;
				if (hist_counter == 0) {
					std::string output = "hist_counter,depth_min,depth_max,count,average_time_in_ns";
					output_streams.line(output, entry.attribute("cpu_id"));
				}
				const size_t hist_bin_count = entry.attribute("hist_bin_count");
				const size_t hist_bin_size  = entry.attribute("hist_bin_size");
				for (size_t bin_index = 0; bin_index < hist_bin_count; bin_index++) {
					size_t depth_min =  bin_index      * hist_bin_size;
					size_t depth_max = (bin_index + 1) * hist_bin_size;
					const auto payload = entry.get_payload();
					size_t count = payload.at(bin_index);
					size_t time_in_ns = payload.at(hist_bin_count + bin_index);
					double average_time_in_ns = count ? static_cast<double>(time_in_ns) / count : 0;
					std::string output = (
						std::to_string(hist_counter) + "," +
						std::to_string(depth_min)    + "," +
						std::to_string(depth_max)    + "," +
						std::to_string(count)        + "," +
						std::to_string(average_time_in_ns)
					);
					output_streams.line(output, entry.attribute("cpu_id"));
				}
				hist_counter ++;
			}
			break;
		case OutputStreams::durations:
			if (entry.attribute("entry_type") == BTE_STACK) {
				static size_t durations_counter = 0;
				if (durations_counter == 0) {
					std::string output = "timer_step,stack_depth,ns_duration,ns_interval";
					output_streams.line(output, entry.attribute("cpu_id"));
				}
				uint64_t interval_ns = (
					previous_entry
					? entry.attribute("tsc_time") - previous_entry->attribute("tsc_time")
					: 0
				);
				std::string output = (
					std::to_string(entry.attribute("timer_step"))   + "," +
					std::to_string(entry.attribute("stack_depth"))  + "," +
					std::to_string(entry.attribute("tsc_duration")) + "," +
					std::to_string(interval_ns)
				);
				output_streams.line(output, entry.attribute("cpu_id"));
				durations_counter ++;
			}
			break;
		}
		previous_entry = &entry;
	}
}

int main(int argc, char * argv []) {
	// TODO: use argp / similar

	if (argc < 2) {
		throw std::runtime_error("missing args: need input file (.btb)");
	}
	std::string tracebuffer_filename { argv[1] };

	if (argc < 3) {
		throw std::runtime_error(std::format(
			"missing args: needs output file (.{})",
			OutputStreams::output_mode_endings_joined("/.")
		));
	}
	std::filesystem::path output_path = argv[2];
	if (!std::filesystem::is_directory(output_path.parent_path())) {
		throw std::runtime_error(std::format(
			"wrong arg: parent {} of output_path '{}' is not a directory!",
			std::string(output_path.parent_path()),
			std::string(output_path)
		));
	}
	OutputStreams output_streams { output_path, false };

	std::optional<std::filesystem::path> symbol_table_directory {};
	if (argc < 4) {
		printf(
			"WARNING: MISSING ARG: if there is no symbol table directory, "
			"you will not be able to interpret your stacks after recompiling the binaries.\n"
		);
	} else {
		symbol_table_directory = argv[3];
		if (!std::filesystem::is_directory(symbol_table_directory->parent_path().parent_path())) {
			throw std::runtime_error(std::format(
				"wrong arg: symbol_table_directory '{}''s parent dir '{}' is not a directory!",
				std::string(*symbol_table_directory),
				std::string( symbol_table_directory->parent_path().parent_path())
			));
		}
	}

	std::string binaries_list_filename = "./data/binaries.list";
	BinariesList binaries_list { binaries_list_filename };

	for (const auto &[name, path] : binaries_list) {
		std::filesystem::path symbol_table_filename;
		if (symbol_table_directory.has_value()) {
			symbol_table_filename = symbol_table_directory.value() / (name + ".symt");
			if (std::filesystem::is_regular_file(symbol_table_filename)) {
				std::cout << std::format("reading symbols for '{}' from '{}'.", name, std::string(symbol_table_filename)) << std::endl;

				binary_symbols.emplace(
					std::piecewise_construct,
					std::forward_as_tuple(name),
					std::forward_as_tuple(symbol_table_filename)
				);
				// if we read it in, we do not want to write it back. that's redundant
				continue;
			}
			std::cout << std::format("couldn't read symbols for '{}' from '{}', doesn't exist.", name, std::string(symbol_table_filename)) << std::endl;
		}

		// we did not hit the continue above, so we need to read it in with ELFIO
		auto const & [new_table_it, was_inserted] = binary_symbols.emplace(
			std::piecewise_construct,
			std::forward_as_tuple(name),
			std::forward_as_tuple(
				name,
				get_elfio_reader(path)
			)
		);
		if (!was_inserted) {
			throw std::runtime_error(std::format(
				"could not store a symbol table for binary '{}', probably a binary name clash or duplication?",
				name
			));
		}
		if (symbol_table_directory.has_value()) {
			// we couln't read it from the file, so let's write it there for next time
			SymbolTable & symbol_table = new_table_it->second;
			std::cout << std::format("exporting symbol table for binary '{}' to '{}'.", name, std::string(symbol_table_filename)) << std::endl;
			symbol_table.export_to_file(symbol_table_filename);
		}
	}

	const std::span<uint64_t> buffer = mmap_file(tracebuffer_filename);
	printf("buffer: %p\n", buffer.data());
	if (buffer.size() == 0) {
		throw std::runtime_error(std::format(
			"file '{}' seems to be empty.",
			tracebuffer_filename
		));
	}

	try {
		interpret(buffer, binaries_list, output_streams);
	} catch (std::exception & e) {
		throw rethrow_error<std::runtime_error>(e, std::format(
			"there was an error in interpreting the data @{}.",
			reinterpret_cast<void *>(buffer.data())
		));
	}
	return 0;
}
