#include <vector>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <iostream>

#include "pathfinding.hpp"
#include "worldmap.hpp"
#include "factorio_io.h"
#include "pos.hpp"

using namespace std;

struct Entry
{
	Pos pos;
	double f;

	Entry(const Pos& p, double f_) : pos(p), f(f_) {}
	bool operator<(const Entry& other) const { return f > other.f; }
};

static double heuristic(const Pos& p, const Pos& goal)
{
	Pos tmp = p-goal;
	return sqrt(tmp.x*tmp.x+tmp.y*tmp.y);
}

vector<Pos> a_star(const Pos& start, const Pos& end, const WorldMap<FactorioGame::walk_t>::Viewport& view, double size)
{
	vector<Entry> openlist;

	unordered_set<Pos> closedlist;

	openlist.push_back( Entry(start, 0.) );
	view.at(start).in_openlist = true;

	while(!openlist.empty())
	{
		auto current = openlist[0];
		pop_heap(openlist.begin(), openlist.end());
		openlist.pop_back();

		if (current.pos == end)
		{
			// found goal.
			
			vector<Pos> result;

			Pos p = current.pos;

			result.push_back(p);
			
			while (p != start)
			{
				p = view.at(p).predecessor;
				cout << p.str() << endl;

				result.push_back(p);
			}


			return result;
		}

		closedlist.insert(current.pos);

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
				if (closedlist.find(successor) != closedlist.end())
					continue;

				auto& succ = view.at(successor);
				double cost = sqrt(step.x*step.x + step.y*step.y);
				double new_g = view.at(current.pos).g_val + cost;

				if (succ.in_openlist && succ.g_val < new_g) // ignore this successor, when a better way is already known
					continue;

				double f = new_g + heuristic(successor, end);
				succ.predecessor = current.pos;
				succ.g_val = new_g;
				
				if (succ.in_openlist)
				{
					for (auto& ent : openlist)
						if (ent.pos == successor)
						{
							ent.f = f;
							break;
						}
					make_heap(openlist.begin(), openlist.end());
				}
				else
				{
					openlist.push_back(Entry(successor, f));
					push_heap(openlist.begin(), openlist.end());
					succ.in_openlist = true;
				}
			}
		}
	}
}
