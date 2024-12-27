#pragma once

#include <cstdint>
#include <algorithm>

#define NDEBUG
#include <cassert>
#undef NDEBUG

template <typename T = uint64_t>
class Range {
private:
	T _begin;
	T _length;
	T _step;

public:
	class iterator {
		T i;
		T step;

	public:
		using iterator_category = std::input_iterator_tag;
		using value_type = T;
		using difference_type = T;
		using pointer = const T*;
		using reference = T;

		explicit iterator(T i, T step) : i(i), step(step) {}
		iterator& operator++()    { i = i + step; return *this; }
		iterator  operator++(int) { iterator retval = *this; ++(*this); return retval; }
		bool operator==(iterator other) const { return i == other.i; }
		bool operator!=(iterator other) const { return !(*this == other); }
		T operator*() const { return i; }
    };

    iterator begin () const { return iterator(_begin, _step); }
    iterator end   () const { return iterator(_begin + _length, _step); }

	Range (T _begin, T _length, T _step = 1) : _begin(_begin), _length(_length), _step(_step) {
		assert(_length >= 0);
	}

	static Range<T> with_end (T _begin, T _end, T _step = 1) {
		return Range<T>(_begin, _end - _begin, _step);
	}
	static Range<T> open_end (T _begin, T _step = 1) {
		return Range<T>(_begin, std::numeric_limits<T>::max() - 1 - _begin, _step);
	}

	T start  () const { return _begin; }
	T stop   () const { return _begin + _length; }
	T length () const { return          _length; }
	T count  () const { return _length / _step;  }

	bool contains (T value) const {
		return (
			start() <= value &&
			value   <  stop()
		);
	}

	Range<T> rounded (T factor) const {
		T begin_rounded = (start()      / factor)     * factor;
		T   end_rounded = ((stop() - 1) / factor + 1) * factor;

		return Range<T> (begin_rounded, end_rounded - begin_rounded, factor);
	}
};
