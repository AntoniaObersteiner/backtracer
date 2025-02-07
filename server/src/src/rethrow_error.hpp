#include <exception>
#include <format>
#include <ranges>
#include <string_view>
#include <vector>

template <typename StringIter>
std::string join(StringIter string_iter, const std::string sep) {
	std::string result = "";
	bool first = true;
	for (const auto & str : string_iter) {
		if (first) {
			first = false;
		} else {
			result += sep;
		}
		result += str;
	}
	return result;
}

template <typename Error>
Error rethrow_error(
	const std::exception & caught,
	const std::string & message
) {
	auto quoted_message = join(
		std::string_view{caught.what()}
		| std::ranges::views::split('\n')
        | std::ranges::views::transform([](auto&& str) {
			return std::string_view(&*str.begin(), std::ranges::distance(str));
		}),
		"\n> "
	);

	return Error(std::format(
		"{}\ncaught error:\n> {}",
		message,
		quoted_message
	));
}
