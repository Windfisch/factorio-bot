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

#pragma once
#include <vector>
#include <boost/heap/binomial_heap.hpp>
#include <limits>
#include "pos.hpp"
#include "worldmap.hpp"

namespace pathfinding
{
	struct Entry
	{
		Pos pos;
		double f; // this is (exact best-known-yet distance to pos) + (estimated distance to target)

		Entry(const Pos& p, double f_) : pos(p), f(f_) {}
		bool operator<(const Entry& other) const { return f > other.f; }
	};

	typedef boost::heap::binomial_heap<Entry>::handle_type openlist_handle_t;
	
	struct walk_t
	{
		bool known;
		bool can_walk;
		bool can_cross;
		int tree_amount;
		double margins[4];


		double g_val;
		Pos predecessor;
		openlist_handle_t openlist_handle;
		bool in_closedlist=false;

		walk_t() : known(false), can_walk(true), can_cross(true), tree_amount(0) {}
		bool water() const { return known && !can_walk; } // FIXME this is a hack
		bool land() const { return known && can_walk; } // FIXME same
	};

}

std::vector<Pos> cleanup_path(const std::vector<Pos>& path);
/* FIXME maybe deprecate those in favor of FactorioGame::blah? */

/** calculates a path from start into the disc around end, with outer radius allowed_distance
  * and inner radius min_distance. If length_limit is positive, the search will abort early
  * if the path is guaranteed to be longer than length_limit. Size specifies the width of
  * the character; 0.5 is usually a good value. */
std::vector<Pos> a_star(const Pos& start, const Pos& end, WorldMap<pathfinding::walk_t>& map, double allowed_distance=1., double min_distance=0., double length_limit=std::numeric_limits<double>::infinity(), double size=0.5);
std::vector<Pos> a_star_raw(const Pos& start, const Pos& end, WorldMap<pathfinding::walk_t>& map, double allowed_distance=1., double min_distance=0., double length_limit=std::numeric_limits<double>::infinity(), double size=0.5);

