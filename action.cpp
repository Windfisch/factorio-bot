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

#include "action.hpp"
#include "pathfinding.hpp"
#include <iostream>
#include <algorithm>
#include "factorio_io.h"
#include <climits>

using namespace std;

constexpr int MINING_DISTANCE = 1;

namespace action {
	int PrimitiveAction::id_counter;
	std::unordered_set<int> PrimitiveAction::pending;
	std::unordered_set<int> PrimitiveAction::finished;

	void WalkTo::start()
	{
		std::vector<Pos> waypoints = a_star(game->players[player].position.to_int(), destination, game->walk_map, 0.4+0.1, allowed_distance);
		subgoals.push_back(unique_ptr<PlayerGoal>(new WalkWaypoints(game,player, waypoints)));

		subgoals[0]->start();
	}

	void WalkWaypoints::execute_impl()
	{
		cout << "WalkWaypoints from " << waypoints[0].str() << " to " << waypoints.back().str() << endl;
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

	void WalkAndPlaceEntity::start()
	{
		std::vector<Pos> waypoints = a_star(game->players[player].position.to_int(),
			pos, game->walk_map, 0.4+0.1, 4., 3.);

		subgoals.push_back(make_unique<WalkWaypoints>(game,player, waypoints));
		subgoals.push_back(make_unique<PlaceEntity>(game,player, item, pos, direction));

		subgoals[0]->start();
	}

	void WalkAndMineObject::start()
	{
		cout << "WalkAndMineObject::start" << endl;

		// plan a path to the object
		std::vector<Pos> waypoints = a_star(game->players[player].position.to_int(),
			obj.pos, game->walk_map, 0.4+0.1, 2.);

		subgoals.push_back(make_unique<WalkWaypoints>(game,player, waypoints));
		subgoals.push_back(make_unique<MineObject>(game,player, obj));

		subgoals[0]->start();
	}

	void WalkAndMineResource::start()
	{
		// plan a path to the centroid of the resource patch
		std::vector<Pos> waypoints = a_star_raw(game->players[player].position.to_int(),
			patch->bounding_box.center(), game->walk_map, 0.4+0.1, 2.);

		// find the first point where we enter the patch
		size_t i;
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
		i = max(ssize_t(i - MINING_DISTANCE), ssize_t(0));
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

	void WalkAndMineResource::on_mined_item(string type, int amount_mined)
	{
		auto it = Resource::types.find(type);
		if (it == Resource::types.end())
			return;

		Resource::type_t type_id = it->second;

		if (type_id == patch->type)
		{
			amount -= amount_mined;
			cout << "mining " << type << ", remaining: " << this->amount << endl;
		}
	}

	void CraftRecipe::execute_impl()
	{
		game->start_crafting(id, player, recipe, count);
	}

	void PlaceEntity::execute_impl()
	{
		game->place_entity(player, item, pos, direction);
		finished.insert(id);
	}

	void PutToInventory::execute_impl()
	{
		game->insert_to_inventory(player, item, amount, entity, inventory_type);
		finished.insert(id);
	}

	void TakeFromInventory::execute_impl()
	{
		game->remove_from_inventory(player, item, amount, entity, inventory_type);
		finished.insert(id);
	}

	void HaveItem::start()
	{
		size_t already_in_inventory = 0; // FIXME

		size_t todo_amount = amount - min(already_in_inventory, size_t(amount)); // ==  max(0, amount - already_in_inventory);
		if (todo_amount == 0)
			return;

		auto& p = game->players[player];
		if (item == "raw-wood")
		{
			// one tree is four raw-wood

			todo_amount = (todo_amount+3)/4; // round up

			int best = INT_MAX;
			vector<Entity> best_nearby_trees;
			for (const auto& closest_tree : game->actual_entities.around(p.position))
			{
				auto dist = (closest_tree.pos-p.position).len();

				if (dist > best) break;

				if (closest_tree.proto->name.substr(0,4) == "tree")
				{
					vector<Entity> nearby_trees;
					for (const auto& tree : game->actual_entities.around(closest_tree.pos))
						if (tree.proto->name.substr(0,4) == "tree")
						{
							nearby_trees.push_back(tree);
							if (nearby_trees.size() >= todo_amount)
								break;
						}

					int quality = int(dist + bounding_box(nearby_trees).diameter()); // we don't need more precision
					if (quality < best)
					{
						best = quality;
						best_nearby_trees = move(nearby_trees);
					}
				}
			}
			
			assert(best_nearby_trees.size() == todo_amount);

			for (const auto& tree : best_nearby_trees)
				subgoals.emplace_back( make_unique<action::WalkAndMineObject>(
					game, player, tree) );
		}
		else if (item == "copper-ore" || item == "iron-ore" || item == "stone" || item == "coal")
		{
			throw runtime_error("not implemented yet"); // FIXME
		}
		else
			assert(false);
	}
}
