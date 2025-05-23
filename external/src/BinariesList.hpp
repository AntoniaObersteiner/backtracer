#pragma once
#include "map_with_errors.hpp"
#include <regex>

class BinariesList : public map_with_errors<std::string, std::string> {
public:
	using Self  = BinariesList;
	using Super = std::map<std::string, std::string>;
	Self  & self  () { return *this; }
	Super & super () { return *reinterpret_cast<Super *>(this); }
	static const std::regex line_regex;

	BinariesList (std::string filename = "./binaries.list");
};

