#pragma once
#include <iostream>
#include <cstdint>
#include <vector>
#include <map>
#include <fstream>
#include <memory>
#include <format>
#include <regex>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>

class BinariesList : public std::map<std::string, std::string> {
public:
	using Self = BinariesList;
	Self & self () { return *this; }
	static const std::regex line_regex;

	BinariesList (std::string filename = "./binaries.list") {
		std::ifstream file (filename);
		if (!file.is_open()) {
			throw std::runtime_error("couldn't open '" + filename + "'!");
		}

		constexpr size_t line_capacity = 1 << 12 - 1;
		char line [line_capacity + 1];

		while (true) {
			file.getline(&line[0], line_capacity + 1);
			size_t bytes_read = file.gcount();
			if (bytes_read <= 0) {
				if (file.eof()) {
					std::cout << "file '" << filename << "' read exhaustively." << std::endl;
					break;
				} else {
					throw std::runtime_error("file '" + filename + "' can't be read from!");
				}
			}

			// empty lines are fine
			if (bytes_read == 1)
				continue;

			// string match result
			std::cmatch match;
			bool matched = std::regex_match(line,  match, line_regex);
			if (!matched) {
				throw std::runtime_error(
					"line '" + std::string(line) + "' is not or the format 'label: filepath'"
				);
			}

			std::string name = match[1];
			std::string path = match[2];
			if (self().contains(name) && self().at(name) != path) {
				throw std::runtime_error(
					"for binary '" + name + "', "
					"there already is path '" + self().at(name) + "' "
					"and now also '" + path + "'?"
				);
			}

			self()[name] = path;
			std::cout << "binary '" << name << "' at path '" << path << "'" << std::endl;
		}
	}
};
const std::regex BinariesList::line_regex { "([^:]+): *([^ ]+)" };

