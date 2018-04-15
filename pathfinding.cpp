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

#pragma GCC optimize "-O2"


#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#ifdef DEBUG_PATHFINDING
#include <iostream>
#endif

#include <boost/heap/binomial_heap.hpp>
#include <cassert>

#include "pathfinding.hpp"
#include "worldmap.hpp"
#include "factorio_io.h"
#include "pos.hpp"
#include "area.hpp"

using namespace std;
using namespace pathfinding;


/** controls the exactness-speed-tradeoff.
 * if set to 1.0, this equals the textbook A*-algorithm, which
 * requires that the heuristic may under- but never overestimate
 * the true distance to the goal. (the direct line as used fulfils
 * this).
 * if set to >1, this results in a potential overestimation of the
 * distance by that factor. This greatly speeds up the algorithm, at
 * the cost of up results worse by up to this factor.
 */
constexpr double OVERAPPROXIMATE=1.1;

static double heuristic(const Pos& p, const Pos& goal)
{
	Pos tmp = p-goal;
	return sqrt(tmp.x*tmp.x+tmp.y*tmp.y)*OVERAPPROXIMATE; // FIXME overapproximation for speed
}

vector<Pos> cleanup_path(const vector<Pos>& path)
{
	vector<Pos> result;
	
	if (path.empty())
		return result;

	Pos dir(0,0);
	for (size_t i=1; i<path.size(); i++)
	{
		Pos newdir = path[i-1] - path[i];
		if (newdir != dir)
			result.push_back(path[i-1]);
	}
	result.push_back(path.back());

	return result;
}

vector<Pos> a_star(const Pos& start, const Pos& end, WorldMap<walk_t>& map, double allowed_distance, double min_distance, double length_limit, double size)
{
	return cleanup_path(a_star_raw(start, end, map, allowed_distance, min_distance, length_limit, size));
}

vector<Pos> a_star_raw(const Pos& start, const Pos& end, WorldMap<walk_t>& map, double allowed_distance, double min_distance, double length_limit, double size)
{
	if (ceil(min_distance) >= allowed_distance)
		throw invalid_argument("ceil(min_distance) must be smaller than allowed distance");
	
	Area view_area(start, end);
	view_area.normalize();
	auto view = map.view(view_area.left_top, view_area.right_bottom, Pos(0,0));

	assert(size<=1.);
	vector<Pos> result;

	boost::heap::binomial_heap<Entry> openlist;

	vector<walk_t*> needs_cleanup;

	view.at(start).openlist_handle = openlist.push(Entry(start,0.));
	needs_cleanup.push_back(&view.at(start));

	int n_iterations = 0;
	while (!openlist.empty())
	{
		auto current = openlist.top();
		openlist.pop();
		n_iterations++;

		if (current.f >= length_limit*OVERAPPROXIMATE) // this (and any subsequent) entry is guaranteed
			break;                                 // to exceed the length_limit.

		if ((current.pos-end).len() <= allowed_distance && (current.pos-end).len() >= min_distance)
		{
			// found goal.

			Pos p = current.pos;

			result.push_back(p);
			
			while (p != start)
			{
				p = view.at(p).predecessor;
				result.push_back(p);
			}

			reverse(result.begin(), result.end());

			#ifdef DEBUG_PATHFINDING
			for (auto pos : result) cout << pos.str() << " - ";
			cout<<endl;
			#endif

			goto a_star_cleanup;
		}

		view.at(current.pos).in_closedlist = true;

		// expand node
		
		Pos steps[] = {Pos(-1,-1), Pos(0, -1), Pos(1,-1),
		               Pos(-1, 0),             Pos(1, 0),
			       Pos(-1, 1), Pos(0,  1), Pos(1, 1)};
		for (const Pos& step : steps)
		{
			Pos successor = current.pos + step;
			bool can_walk = false;

			if (step.x == 0) // walking in vertical direction
			{
				auto x = current.pos.x;
				auto y = min(current.pos.y, successor.y);
				can_walk = view.at(x-1, y).margins[EAST] >= size/2 && view.at(x, y).margins[WEST] >= size/2 && view.at(x-1,y).can_walk && view.at(x,y).can_walk;
			}
			else if (step.y == 0) // walking in horizontal direction
			{
				auto x = min(current.pos.x, successor.x);
				auto y = current.pos.y;
				can_walk = view.at(x, y-1).margins[SOUTH] >= size/2 && view.at(x, y).margins[NORTH] >= size/2 && view.at(x,y-1).can_walk && view.at(x,y).can_walk;
			}
			else // walking diagonally
			{
				auto x = min(current.pos.x, successor.x);
				auto y = min(current.pos.y, successor.y);
				const auto& v = view.at(x,y);
				can_walk = (v.margins[0]>=0.5 && v.margins[1]>=0.5 && v.margins[2]>=0.5 && v.margins[3]>=0.5) && v.can_walk && (view.at(x+step.x, y).can_walk || view.at(x, y+step.y).can_walk);
			}

			if (can_walk)
			{
				auto& succ = view.at(successor);
				if (succ.in_closedlist)
					continue;

				double cost = sqrt(step.x*step.x + step.y*step.y);
				double new_g = view.at(current.pos).g_val + cost;

				if (succ.openlist_handle != openlist_handle_t() && succ.g_val < new_g) // ignore this successor, when a better way is already known
					continue;

				double f = new_g + heuristic(successor, end);
				succ.predecessor = current.pos;
				succ.g_val = new_g;
				
				if (succ.openlist_handle != openlist_handle_t())
				{
					openlist.update(succ.openlist_handle, Entry(successor, f));
				}
				else
				{
					succ.openlist_handle = openlist.push(Entry(successor, f));
					needs_cleanup.push_back(&succ);
				}
			}
		}
	}

a_star_cleanup:
	
	for (walk_t* w : needs_cleanup)
	{
		w->openlist_handle = pathfinding::openlist_handle_t();
		w->in_closedlist = false;
	}

	#ifdef DEBUG_PATHFINDING
	cout << "took " << n_iterations << " iterations or " << (n_iterations / (start-end).len()) << " it/dist" << endl;
	#endif

	return result;
}
