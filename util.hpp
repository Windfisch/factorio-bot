/*
 * Copyright (c) 2017 Florian Jung
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

#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"

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
		return *iter;
	else
		return default_value;
}

#pragma GCC diagnostic pop
