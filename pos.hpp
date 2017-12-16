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
#include <type_traits>
#include <cmath>
#include "util.hpp"

constexpr int tileidx(int x)
{
	return ((x % 32) + 32) % 32;
}

constexpr int chunkidx(int x)
{
	return (x>=0) ? x/32 : (x-31)/32;
}

template<typename T>
struct Pos_
{
	T x,y;
	Pos_(T x_, T y_) : x(x_), y(y_) {}
	Pos_() : x(0), y(0) {}
	template <typename U> Pos_(const Pos_<U>& other) : x(T(other.x)), y(T(other.y)) {}

	Pos_<T> operator+(const Pos_<T>& b) const { return Pos_<T>(x+b.x, y+b.y); }
	Pos_<T> operator-(const Pos_<T>& b) const { return Pos_<T>(x-b.x, y-b.y); }
	Pos_<T> operator*(T f) const { return Pos_<T>(x*f, y*f); }
	
	
	Pos_<T> operator/(T f) const;

	bool operator==(const Pos_<T> that) const
	{
		#pragma GCC diagnostic push
		#pragma GCC diagnostic ignored "-Wfloat-equal"
		return this->x == that.x && this->y == that.y;
		#pragma GCC diagnostic pop
	}

	bool operator!=(const Pos_<T> that) const { return !operator==(that); }

	bool operator< (const Pos_<T>& that) const { return this->x <  that.x && this->y <  that.y; }
	bool operator<=(const Pos_<T>& that) const { return this->x <= that.x && this->y <= that.y; }
	bool operator> (const Pos_<T>& that) const { return this->x >  that.x && this->y >  that.y; }
	bool operator>=(const Pos_<T>& that) const { return this->x >= that.x && this->y >= that.y; }

	double len() const { return std::sqrt(x*x+y*y); }

	std::string str() const { return std::to_string(x) + "," + std::to_string(y); }

	Pos_<int> to_int() const { return Pos_<int>(int(std::round(x)), int(std::round(y))); }
	Pos_<int> to_int_floor() const { return Pos_<int>(int(std::floor(x)), int(std::floor(y))); }
	Pos_<double> to_double() const { return Pos_<double>(x,y); }

	static Pos_<T> tile_to_chunk(const Pos_<T>& p) { return Pos_<T>(chunkidx(p.x), chunkidx(p.y)); }
	static Pos_<T> tile_to_chunk_ceil(const Pos_<T>& p) { return Pos_<T>(chunkidx(p.x+31), chunkidx(p.y+31)); }
	static Pos_<T> chunk_to_tile(const Pos_<T>& p) { return Pos_<T>(p.x*32, p.y*32); }
};

template <> inline Pos_<int> Pos_<int>::operator/(int f) const { return Pos_<int>(sane_div(x,f), sane_div(y,f)); }
template <> inline Pos_<double> Pos_<double>::operator/(double f) const { return Pos_<double>(x/f,y/f); }

typedef Pos_<int> Pos;
typedef Pos_<double> Pos_f;

namespace std {
	template <> struct hash<Pos>
	{
		typedef Pos argument_type;
		typedef std::size_t result_type;
		result_type operator()(argument_type const& p) const
		{
			result_type const h1( std::hash<int>{}(p.x) );
			result_type const h2( std::hash<int>{}(p.y) );
			return h1 ^ (h2 << 1);
		}
	};
}

static Pos directions4[4] = { Pos(0,-1), Pos(1,0), Pos(0,1), Pos(-1,0) };
