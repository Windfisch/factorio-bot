#include <string>
#include <vector>
#include <tuple>

template <typename T> T convert(std::string const& s)
{
	if constexpr (std::is_integral_v<T>)
		return T(stoi(s));
	else if constexpr (std::is_floating_point_v<T>)
		return T(stod(s));
	else if constexpr (std::is_same_v<T, std::string>)
		return s;
	else
		static_assert(sizeof(T) != sizeof(T), "only integral, floating point or string types are supported");
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

// Usage: auto [foo, bar, baz] = unpack<int, float, string>(my_vector)
