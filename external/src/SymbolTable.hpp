#pragma once
#include <filesystem>
#include <optional>
#include <regex>

#include "map_with_errors.hpp"
#include "elfi.hpp"
#include "Range.hpp"

class SymbolPage;
class Symbol {
public:
	std::string name;
	std::string binary;

	Range<> instruction_addresses;

	Symbol (
		const std::string & name,
		const std::string & binary,
		const Range<> & instruction_addresses
	) : name(name),
		binary(binary),
		instruction_addresses(instruction_addresses)
	{}

	static const std::string file_line_regex_string;
	static const std::regex  file_line_regex;

	static Symbol from_file_line (
		const std::string & file_line
	) {
		std::smatch match;
		bool matched = std::regex_match(file_line, match, file_line_regex);
		if (!matched) {
			throw std::runtime_error(std::format(
				"could not match line '{}' with file_line_regex '{}'.",
				file_line,
				file_line_regex_string
			));
		}

		const std::string binary = match[1];
		const std::string name = match[4];

		size_t start, end;
		std::string start_str { match[2] };
		std::string   end_str { match[3] };
		std::from_chars(start_str.data(), start_str.data() + start_str.size(), start, 16);
		std::from_chars(  end_str.data(),   end_str.data() +   end_str.size(),   end, 16);

		Range<> instruction_addresses = Range<size_t>::with_end(start, end);
		return Symbol(name, binary, instruction_addresses);
	}

	std::string to_file_line () const {
		return std::format(
			"{}\t{:016x}\t{:016x}\t{}",
			binary,
			instruction_addresses.start(),
			instruction_addresses.stop(),
			name
		);
	}

	bool starts_in_page(const size_t page_address) const;

	std::string label () const {
		return binary + "`" + demangle(name);
	}

	std::string dbg () const {
		return binary + "`" + demangle(name) + ":" + instruction_addresses.to_string(std::hex);
	}
};

class SymbolPage : public std::vector<Symbol> {
public:
	using Super = std::vector<Symbol>;
	using Self = SymbolPage;
	Self        & self  ()       { return *this; }
	Self  const & self  () const { return *this; }
	Super       & super ()       { return static_cast<      Super &>(*this); }
	Super const & super () const { return static_cast<const Super &>(*this); }

	SymbolPage () = default;
	SymbolPage (const std::vector<Symbol> & symbols) : Super(symbols) {}

	std::optional<Symbol> find_symbol (const uint64_t instruction_pointer) const {
		auto found = std::find_if (super().cbegin(), super().cend(),
			[instruction_pointer] (const Symbol & symbol) {
				return symbol.instruction_addresses.contains(instruction_pointer);
			}
		);
		if (found == super().cend())
			return std::optional<Symbol>();
		return std::optional<Symbol>(*found);
	}
};

class SymbolTable : public map_with_errors<uint64_t, SymbolPage> {
public:
	using Super = map_with_errors<uint64_t, SymbolPage>;
	using Self = SymbolTable;
	Self        & self  ()       { return *this; }
	Self  const & self  () const { return *this; }
	Super       & super ()       { return static_cast<      Super &>(*this); }
	Super const & super () const { return static_cast<const Super &>(*this); }

private:
	std::string binary;

public:
	SymbolTable (
		const std::string & binary,
		const ELFIO::elfio & reader
	);

	SymbolTable (const std::string & symbol_table_filename);

	void export_to_file (const std::filesystem::path & symbol_table_filename) const;

	void insert_symbol(const Symbol & symbol);

	std::optional<Symbol> find_symbol (const uint64_t instruction_pointer) const;
};

extern map_with_errors<std::string, SymbolTable> binary_symbols;
