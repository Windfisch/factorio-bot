/* These utility functions are dual-licensed under both the
 * terms of the MIT and the GPL3 license. You can choose which
 * of those licenses you want to use. Note that the MIT option
 * only applies to this file, and not to the rest of
 * factorio-bot (unless stated otherwise).
 */

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

/*
 * MIT License
 *
 * Copyright (c) 2017, 2018 Florian Jung
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this templated WorldList implementation and associated
 * documentation files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished to do
 * so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <cmath>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

template <typename Container, typename Func>
std::string join_string (const Container& container, Func function)
{
	std::string result;
	bool first = true;
	for (const auto& x : container)
	{
		if (!first) result += ",";
		first = false;
		result += function(x);
	}
	return result;
}

constexpr int sane_div(int a, unsigned b)
{
	return (a>=0) ? (a/b) : (signed(a-b+1)/signed(b));
}

constexpr int sane_mod(int a, unsigned b)
{
	return ((a%b)+b)%b;
}

auto min_element_if = [](auto& container, auto predicate, auto compare)
{
	auto best = container.end();

	for (auto it=container.begin(); it!=container.end(); it++)
		if (predicate(*it))
		{
			if (best==container.end() || compare(*it, *best))
				best = it;
		}
	
	return best;
};

static std::string strpad(std::string s, size_t n)
{
	if (s.length() >= n) return s;
	else return s + std::string(n-s.length(),' ');
}

template <typename container_type> void accumulate_map(container_type& accumulator, const container_type& increment)
{
	for (const auto& [key,value] : increment)
	{
		auto iter = accumulator.find(key);
		if (iter == accumulator.end())
			accumulator[key] = value;
		else
			accumulator[key] += value;
	}
}

template <typename container_type, typename iterator_type> iterator_type unordered_erase(container_type& container, iterator_type iter)
{
	if (iter == container.end()-1)
	{
		container.pop_back();
		return container.end();
	}
	else
	{
		std::swap(*iter, container.back());
		container.pop_back();
		return iter;
	}
}


template <typename T, typename K, K T::*key, typename Comparator = std::equal_to<K>> class vector_map : public std::vector<T>
{
	public:
		size_t find(K what) const
		{
			Comparator comp;

			for (size_t i=0; i< std::vector<T>::size(); i++)
				if ( comp((*this)[i].*key, what) )
					return i;
			return SIZE_MAX;
		}

		T& get(K what)
		{
			auto idx = find(what);
			if (idx == SIZE_MAX)
			{
				std::vector<T>::emplace_back();
				std::vector<T>::back().*key = what;
				idx = std::vector<T>::size() - 1;
			}
			return (*this)[idx];
		}
};

template <typename T, typename U>
inline bool equals(const std::weak_ptr<T>& t, const std::weak_ptr<U>& u)
{
    return !t.owner_before(u) && !u.owner_before(t);
}


template <typename T, typename U>
inline bool equals(const std::weak_ptr<T>& t, const std::shared_ptr<U>& u)
{
    return !t.owner_before(u) && !u.owner_before(t);
}

template <typename T, typename U=T> struct weak_ptr_equal
{
	inline bool operator()(const T& a, const U& b)
	{
		return equals(a,b);
	}
};

template <typename Container, typename Comparator = std::equal_to<typename Container::value_type>>
	std::vector< std::pair<size_t, typename Container::value_type> > compact_sequence(const Container& sequence)
	{
		std::vector< std::pair<size_t, typename Container::value_type> > result;

		if (sequence.empty())
			return result;

		typename Container::value_type last = *sequence.begin();
		size_t count = 0;

		for (const typename Container::value_type iter : sequence)
		{
			if (iter == last)
				count++;
			else
			{
				result.emplace_back(count, last);
				count = 1;
				last = iter;
			}
		}

		result.emplace_back(count, last);

		return result;
	}

template <typename MapContainer> typename MapContainer::mapped_type get_or(const MapContainer& container, typename MapContainer::key_type key, typename MapContainer::mapped_type default_value = typename MapContainer::mapped_type())
{
	if (auto iter = container.find(key); iter != container.end())
		return iter->second;
	else
		return default_value;
}

template <typename Container, typename T> bool contains(const Container& container, const T& search_for)
{
	return container.find(search_for) != container.end();
}
template <typename Container, typename T> bool contains_vec(const Container& container, const T& search_for)
{
	return find(container.begin(), container.end(), search_for) != container.end();
}

inline int ceili(double f) { return int(std::ceil(f)); }
inline int floori(double f) { return int(std::floor(f)); }

#pragma GCC diagnostic pop
