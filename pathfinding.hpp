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
#include <vector>
#include <boost/heap/binomial_heap.hpp>
#include "pos.hpp"
#include "worldmap.hpp"

namespace pathfinding
{
	struct Entry
	{
		Pos pos;
		double f;

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
std::vector<Pos> a_star(const Pos& start, const Pos& end, WorldMap<pathfinding::walk_t>& map, double allowed_distance=0., double min_distance=0., double size=0.5);
std::vector<Pos> a_star_raw(const Pos& start, const Pos& end, WorldMap<pathfinding::walk_t>& map, double allowed_distance=0., double min_distance=0., double size=0.5);

