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
using std::min;
using std::abs;

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

double distance(Pos_f a, Area_f b)
{
	if (b.contains(a)) return 0.;
	
	if (b.left_top.x <= a.x && a.x <= b.right_bottom.x)
		return min(abs(a.y - b.left_top.y), abs(a.y - b.right_bottom.y));
	if (b.left_top.y <= a.y && a.y <= b.right_bottom.y)
		return min(abs(a.x - b.left_top.x), abs(a.x - b.right_bottom.x));
	
	Pos_f right_top( b.right_bottom.x, b.left_top.y );
	Pos_f left_bottom( b.left_top.x, b.right_bottom.y );
	return min(
		min( (a-b.left_top).len(), (a-b.right_bottom).len() ),
		min( (a-right_top).len(), (a-left_bottom).len() ));
}

template struct Area_<int>;
template struct Area_<double>;
