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
#include <type_traits>
#include <string>
#include <cmath>
#include <algorithm>
#include "pos.hpp"
#include "defines.h"
#include <assert.h>
#include <vector>

template<typename T> struct Area_
{
	Pos_<T> left_top;
	Pos_<T> right_bottom;

	Area_() {}
	Area_(T x1, T y1, T x2, T y2) : left_top(x1,y1), right_bottom(x2,y2)  {}
	Area_(const Pos_<T>& lt, const Pos_<T>& rb) : left_top(lt), right_bottom(rb) {}
	Area_(const std::vector<Pos_<T>>& positions)
	{
		assert(!positions.empty());
		// calculate the bounding box
		left_top = positions[0];
		right_bottom = positions[0];
		for (const auto& p : positions)
		{
			if (p.x < left_top.x)
				left_top.x = p.x;
			if (p.x >= right_bottom.x)
				right_bottom.x = p.x+1; // FIXME what to do with floats here :/?
			if (p.y < left_top.y)
				left_top.y = p.y;
			if (p.y >= right_bottom.y)
				right_bottom.y = p.y+1;
		}
	}
	Area_(std::string str);
	template <typename U> Area_(const Area_<U>& other) : left_top(other.left_top), right_bottom(other.right_bottom) {}
	
	std::string str() const;

	bool operator==(const Area_<T>& other) const { return this->left_top==other.left_top && this->right_bottom==other.right_bottom; }

	void normalize()
	{
		if (left_top.x > right_bottom.x)
			std::swap(left_top.x, right_bottom.x);
		if (left_top.y > right_bottom.y)
			std::swap(left_top.y, right_bottom.y);
	}

	Pos_<T> center() const { return (left_top + right_bottom) / 2; }

	bool contains(const Pos_<T>& p) const { return left_top <= p && p < right_bottom; }
	bool contains_x(T x) const { return left_top.x <= x && x < right_bottom.x; }
	bool contains_y(T y) const { return left_top.y <= y && y < right_bottom.y; }

	T diameter() const { return std::max(right_bottom.x - left_top.x, right_bottom.y - left_top.y); }
	
	template <typename U=T> typename std::enable_if< std::is_floating_point<U>::value, Area_<int> >::type
	/*Area_<int>*/ outer() const
	{
		return Area_<int>( Pos_<int>(int(std::floor(left_top.x)), int(std::floor(left_top.y))),
		                   Pos_<int>(int(std::ceil(right_bottom.x)), int(std::ceil(right_bottom.y))) );
	}

	Area_<T> rotate(dir4_t rot) const // assumes that the box is originally in NORTH orientation. i.e., passing rot=NORTH will do nothing, passing EAST will rotate 90deg clockwise etc
	{
		assert(rot == NORTH || rot == EAST || rot == SOUTH || rot == WEST);
		switch (rot)
		{
			case NORTH: return *this;
			case EAST: return Area_<T>( Pos_<T>(-right_bottom.y, left_top.x), Pos_<T>(-left_top.y, right_bottom.x) );
			case SOUTH: return Area_<T>( Pos_<T>(-right_bottom.x, -right_bottom.y), Pos_<T>(-left_top.x, -left_top.y) );
			case WEST: return Area_<T>( Pos_<T>(left_top.y, -right_bottom.x), Pos_<T>(right_bottom.y, -left_top.x) );
		}
		// can't be reached
		assert(false);
	}

	Area_<T> shift(Pos_<T> offset) const { return Area_<T>(left_top+offset, right_bottom+offset); }
	Area_<T> expand(T radius) const { return Area_<T>(left_top-Pos_<T>(radius,radius), right_bottom+Pos_<T>(radius,radius)); }
	Area_<T> expand(Pos_<T> point) const { return Area_<T>( std::min(left_top.x, point.x), std::min(left_top.y, point.y), std::max(right_bottom.x, point.x), std::max(right_bottom.y, point.y) ); }
	T radius() const { return std::max( std::max( left_top.x, left_top.y), std::max( right_bottom.x, right_bottom.y) ); }
	Area_<T> intersect(Area_<T> other) const
	{
		return Area_<T>(
				Pos_<T>(
					std::max(this->left_top.x, other.left_top.x),
					std::max(this->left_top.y, other.left_top.y)
				),
				Pos_<T>(
					std::min(this->right_bottom.x, other.right_bottom.x),
					std::min(this->right_bottom.y, other.right_bottom.y)
				)
			);
	}
};

typedef Area_<int> Area;
typedef Area_<double> Area_f;

template <> Area_<int>::Area_(std::string str);
template <> Area_<double>::Area_(std::string str);
extern template struct Area_<int>;
extern template struct Area_<double>;
