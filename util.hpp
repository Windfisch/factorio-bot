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

#pragma GCC diagnostic pop
