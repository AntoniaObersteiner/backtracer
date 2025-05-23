#pragma once

#include <map>
#include <stdexcept>
// #include <utility> // c++23: std::unreachable

template<
	class Key,
	class T,
	class Compare = std::less<Key>,
	class Allocator = std::allocator<std::pair<const Key, T>>
>
class map_with_errors : public std::map <Key, T, Compare, Allocator> {
private:
	void _throw_out_of_bounds(const Key & key) const {
		std::string some_keys = "";
		std::string sep = "";
		int patience = 10;
		for (const auto & [key, value] : super()) {
			some_keys += std::format("{}{}", sep, key);
			sep = ", ";
			patience --;
			if (patience <= 0) {
				some_keys += "...";
				break;
			}
		}
		throw std::out_of_range(std::format(
			"this map has no key '{}'. But it has: {}",
			key,
			some_keys
		));
	}
public:
	using Super = std::map <Key, T, Compare, Allocator>;
	Super       & super ()       { return *static_cast<Super       *>(this); }
	Super const & super () const { return *static_cast<Super const *>(this); }
	/*
	T & operator[] (const Key &  key) { return super().operator [](key); }
	T & operator[] (      Key && key) { return super().operator [](key); }
	*/
	T       & at   (const Key &  key)       {
		if (super().contains(key)) return super().at(key);
		else _throw_out_of_bounds(key);
		// std::unreachable(); // c++23
	}
	T const & at   (const Key &  key) const {
		if (super().contains(key)) return super().at(key);
		else _throw_out_of_bounds(key);
		// std::unreachable(); // c++23
	}
};
