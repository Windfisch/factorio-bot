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

#include <string>
#include <cstdio>
#include "area.hpp"

using std::string;
using std::sscanf;

template <>
Area_<int>::Area_(string str)
{
	sscanf(str.c_str(), "%i,%i;%i,%i", &left_top.x, &left_top.y, &right_bottom.x, &right_bottom.y);
}

template <>
Area_<double>::Area_(string str)
{
	sscanf(str.c_str(), "%lf,%lf;%lf,%lf", &left_top.x, &left_top.y, &right_bottom.x, &right_bottom.y);
}

template <typename T>
string Area_<T>::str() const
{
	return left_top.str() + " -- " + right_bottom.str();
}

template struct Area_<int>;
template struct Area_<double>;
