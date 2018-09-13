/*
 * Copyright (c) 2017, 2018 Florian Jung
 *
 * This file is part of factorio-bot.
 *
 * factorio-bot is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License,
 * version 3, as published by the Free Software Foundation.
 *
 * factorio-bot is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with factorio-bot. If not, see <http://www.gnu.org/licenses/>.
 */

#include <string>
#include <vector>
#include <sstream>
#include <tuple>

// TODO FIXME: iterable thingy

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
