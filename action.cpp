#include "action.hpp"
#include "pathfinding.hpp"
#include <iostream>
#include <algorithm>
#include "factorio_io.h"

using namespace std;

constexpr int MINING_DISTANCE = 2;

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

	void MineObject::execute_impl()
	{
		game->set_mining_target(id, player, obj);
	}

	void MineObject::abort()
	{
		game->unset_mining_target(player);
		mark_finished(id);
	}

	void WalkAndMineResource::start()
	{
		// plan a path to the centroid of the resource patch
		std::vector<Pos> waypoints = a_star_raw(game->players[player].position.to_int(),
			patch->bounding_box.center(), game->walk_map, 0.4+0.1);

		// find the first point where we enter the patch
		ssize_t i;
		for (i=0; i<waypoints.size(); i++)
		{
			auto& tile = game->resource_map.at(waypoints[i]);
			if (tile.type != Resource::NONE && tile.resource_patch.lock() == patch)
				break;
		}
		
		// get the tile we want to mine at. tile.entity is the imporant part.
		auto& tile = game->resource_map.at(waypoints[i]);
		assert(tile.type == patch->type && tile.resource_patch.lock() == patch);

		// because we have a certain mining distance, we can stop walking a bit before arriving.
		i = max(i - MINING_DISTANCE, ssize_t(0));
		waypoints.resize(i+1);

		// we need to cleanup the path, because it came from astar_raw(), not astar().
		waypoints = cleanup_path(waypoints);

		subgoals.push_back(make_unique<WalkWaypoints>(game,player, waypoints));
		subgoals.push_back(make_unique<MineObject>(game,player, tile.entity));

		subgoals[0]->start();
	}

	void WalkAndMineResource::tick()
	{
		if (amount <= 0)
		{
			assert(dynamic_cast<MineObject*>(subgoals[current_subgoal].get()));
			subgoals[current_subgoal]->abort();
		}

		CompoundGoal::tick();
	}

	void WalkAndMineResource::on_mined_item(string type, int amount)
	{
		auto it = Resource::types.find(type);
		if (it == Resource::types.end())
			return;

		Resource::type_t type_id = it->second;

		if (type_id == patch->type)
		{
			this->amount -= amount;
			cout << "mining " << type << ", remaining: " << this->amount << endl;
		}
	}

	void CraftRecipe::execute_impl()
	{
		game->start_crafting(id, player, recipe, count);
	}
}
