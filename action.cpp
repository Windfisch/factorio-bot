#include "action.hpp"
#include "pathfinding.hpp"
#include <iostream>
#include "factorio_io.h"

using namespace std;

namespace action {
	int PrimitiveAction::id_counter;
	std::unordered_set<int> PrimitiveAction::pending;
	std::unordered_set<int> PrimitiveAction::finished;

	void WalkTo::start()
	{
		std::vector<Pos> waypoints = a_star(game->players[player].position.to_int(), destination, game->walk_map, 0.4+0.1);
		subgoals.push_back(unique_ptr<PlayerGoal>(new WalkWaypoints(game,player, waypoints)));

		subgoals[0]->start();
	}

	void WalkWaypoints::execute_impl()
	{
		game->set_waypoints(id, player, waypoints);
	}
}
