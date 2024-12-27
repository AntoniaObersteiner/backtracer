#pragma once
#include <optional>
#include <map>

#include "elfi.hpp"
#include "Range.hpp"

class Symbol {
public:
	const std::string name;
	const std::string binary;

	const Range<> instruction_addresses;

	Symbol (
		const std::string & name,
		const std::string & binary,
		const Range<> & instruction_addresses
	) : name(name),
		binary(binary),
		instruction_addresses(instruction_addresses)
	{}

	std::string label () const {
		return binary + "/" + name;
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

class SymbolTable : private std::map <uint64_t, SymbolPage> {
public:
	using Super = std::map<uint64_t, SymbolPage>;
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

	SymbolTable () : binary("<uninitialized SymbolTable>") {}

	std::optional<Symbol> find_symbol (const uint64_t instruction_pointer) const;
};

extern std::map<std::string, SymbolTable> binary_symbols;
