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
	};

}

std::vector<Pos> cleanup_path(const std::vector<Pos>& path);
std::vector<Pos> a_star(const Pos& start, const Pos& end, WorldMap<pathfinding::walk_t>& map, double size);
std::vector<Pos> a_star_raw(const Pos& start, const Pos& end, WorldMap<pathfinding::walk_t>& map, double size);

