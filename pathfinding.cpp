#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <iostream>

#include <boost/heap/binomial_heap.hpp>

#include "pathfinding.hpp"
#include "worldmap.hpp"
#include "factorio_io.h"
#include "pos.hpp"
#include "area.hpp"

using namespace std;
using namespace pathfinding;

static double heuristic(const Pos& p, const Pos& goal)
{
	Pos tmp = p-goal;
	return sqrt(tmp.x*tmp.x+tmp.y*tmp.y)*1.1; // FIXME overapproximation for speed
}

vector<Pos> a_star(const Pos& start, const Pos& end, WorldMap<walk_t>& map, double size)
{
	Area view_area(start, end);
	view_area.normalize();
	auto view = map.view(view_area.left_top, view_area.right_bottom, Pos(0,0));

	assert(size<=1.);
	vector<Pos> result;

	boost::heap::binomial_heap<Entry> openlist;

	vector<walk_t*> needs_cleanup;

	view.at(start).openlist_handle = openlist.push(Entry(start,0.));
	needs_cleanup.push_back(&view.at(start));

	while(!openlist.empty())
	{
		auto current = openlist.top();
		openlist.pop();

		if (current.pos == end)
		{
			// found goal.

			Pos p = current.pos;
			Pos dir(0,0);

			result.push_back(p);
			
			while (p != start)
			{
				Pos oldp = p;
				p = view.at(p).predecessor;

				Pos newdir = p-oldp;
				if (newdir == dir)
					result.back() = p;
				else
					result.push_back(p);

				dir = newdir;
			}

			reverse(result.begin(), result.end());

			for (auto p : result) cout << p.str() << " - ";
			cout<<endl;

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
				can_walk = (view.at(x-1, y).margins[EAST] + view.at(x, y).margins[WEST]) >= size && view.at(x-1,y).can_walk && view.at(x,y).can_walk;
			}
			else if (step.y == 0) // walking in horizontal direction
			{
				auto x = min(current.pos.x, successor.x);
				auto y = current.pos.y;
				can_walk = (view.at(x, y-1).margins[SOUTH] + view.at(x, y).margins[NORTH]) >= size && view.at(x,y-1).can_walk && view.at(x,y).can_walk;
			}
			else // walking diagonally
			{
				auto x = min(current.pos.x, successor.x);
				auto y = min(current.pos.y, successor.y);
				const auto& v = view.at(x,y);
				can_walk = (v.margins[0]>=0.5 && v.margins[1]>=0.5 && v.margins[2]>=0.5 && v.margins[3]>=0.5) && v.can_walk;
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

	return result;
}
