#include <string>
#include <vector>
#include <sstream>
#include <tuple>

static std::vector<std::string> split(const std::string& data, char delim=' ')
{
	std::istringstream str(data);
	std::string entry;
	std::vector<std::string> result;

	if (data.empty())
		return result;

	do {
		getline(str, entry, delim);
		result.push_back(std::move(entry));
	} while (!str.eof());
	
	return result;
}

template <typename T> T convert(std::string const& s)
{
	if constexpr (std::is_integral_v<T>)
		return T(stoi(s));
	else if constexpr (std::is_floating_point_v<T>)
		return T(stod(s));
	else if constexpr (std::is_same_v<T, std::string>)
		return s;
	else
		return T(s);
}

template <typename... Ts, std::size_t... Is>
	std::tuple<Ts...> unpack(std::vector<std::string> const& v, std::index_sequence<Is...>)
{
	return {convert<Ts>(v[Is])...};
}

template <typename... Ts> std::tuple<Ts...> unpack(std::vector<std::string> const& v)
{
	return unpack<Ts...>(v, std::index_sequence_for<Ts...>{});
}

template <typename... Ts> std::tuple<Ts...> unpack(const std::string& str, char delim=' ')
{
	return unpack<Ts...>(split(str, delim));
}

// Usage: auto [foo, bar, baz] = unpack<int, float, string>(my_vector)
