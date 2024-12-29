#pragma once
#include <map>
#include <regex>

class BinariesList : public std::map<std::string, std::string> {
public:
	using Self = BinariesList;
	Self & self () { return *this; }
	static const std::regex line_regex;

	BinariesList (std::string filename = "./binaries.list");
};

