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
#include "constants.h"
#include <iostream>
#include <algorithm>
#include "factorio_io.h"
#include <climits>

using namespace std;

constexpr int MINING_DISTANCE = 1;

namespace action {
	Registry registry;

	void Registry::cleanup()
	{
		for (iterator iter = begin(); iter != end();)
		{
			if (iter->second.expired())
				iter = erase(iter);
			else
				++iter;
		}
	}

	shared_ptr<sched::Task> Registry::get(int id)
	{
		auto iter = find(id);
		if (iter == end())
			return nullptr;
		else
			return iter->second.lock();
	}

	int PrimitiveAction::id_counter;
	std::unordered_set<int> PrimitiveAction::pending;
	std::unordered_set<int> PrimitiveAction::finished;

	void WalkTo::start()
	{
		std::vector<Pos> waypoints = a_star(game->players[player].position.to_int(), destination, game->walk_map, allowed_distance);
		subactions.push_back(unique_ptr<ActionBase>(new WalkWaypoints(game,player, waypoints)));

		subactions[0]->start();
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

/*
	void WalkAndPlaceEntity::start()
	{
		std::vector<Pos> waypoints = a_star(game->players[player].position.to_int(),
			pos, game->walk_map, 4., 3.);

		subactions.push_back(make_unique<WalkWaypoints>(game,player, waypoints));
		subactions.push_back(make_unique<PlaceEntity>(game,player, item, pos, direction));

		subactions[0]->start();
	}

	void WalkAndMineObject::start()
	{
		cout << "WalkAndMineObject::start" << endl;

		// plan a path to the object
		std::vector<Pos> waypoints = a_star(game->players[player].position.to_int(),
			obj.pos, game->walk_map, 2.);

		subactions.push_back(make_unique<WalkWaypoints>(game,player, waypoints));
		subactions.push_back(make_unique<MineObject>(game,player, obj));

		subactions[0]->start();
	}

	void WalkAndMineResource::start()
	{
		// plan a path to the centroid of the resource patch
		std::vector<Pos> waypoints = a_star_raw(game->players[player].position.to_int(),
			patch->bounding_box.center(), game->walk_map, 2.);

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

		subactions.push_back(make_unique<WalkWaypoints>(game,player, waypoints));
		subactions.push_back(make_unique<MineObject>(game,player, tile.entity));

		subactions[0]->start();
	}

	void WalkAndMineResource::tick()
	{
		if (amount <= 0)
		{
			assert(dynamic_cast<MineObject*>(subactions[current_subaction].get()));
			subactions[current_subaction]->abort();
		}

		CompoundAction::tick();
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
*/
	void CraftRecipe::execute_impl()
	{
		game->start_crafting(id, player, recipe->name, count);
	}

	void PlaceEntity::execute_impl()
	{
		game->place_entity(player, item->name, pos, direction);
		finished.insert(id);
	}

	void PutToInventory::execute_impl()
	{
		game->insert_to_inventory(player, item->name, amount, entity, inventory_type);
		finished.insert(id);
	}

	void TakeFromInventory::execute_impl()
	{
		game->remove_from_inventory(player, item->name, amount, entity, inventory_type);
		finished.insert(id);
	}

	std::pair<Pos, Clock::duration> WalkTo::walk_result(Pos current_position) const
	{
		// TODO FIXME: if we've a detailed path, return that one
		return pair(destination, chrono::milliseconds( int(1000*(destination-current_position).len() / WALKING_SPEED) ));
	}

	std::pair<Pos, Clock::duration> WalkWaypoints::walk_result(Pos current_position) const
	{
		if (waypoints.empty())
			return pair(current_position, Clock::duration(0));

		float seconds = 0.f;
		for (size_t i = 0; i < waypoints.size() - 1; i++)
		{
			const Pos& from = waypoints[i];
			const Pos& to = waypoints[i+1];
			seconds += (from-to).len() / WALKING_SPEED;
		}

		return pair(waypoints.back(), chrono::milliseconds(int(1000*seconds)));
	}

	item_balance_t MineObject::inventory_balance() const
	{
		return obj.proto->mine_results;
	}

	item_balance_t CompoundAction::inventory_balance() const
	{
		item_balance_t result;
		for (const auto& action : subactions)
		{
			for (const auto& [item,amount] : action->inventory_balance())
				result[item] += amount;
		}
		return result;
	}

	string CompoundAction::str() const
	{
		string result = "CompoundAction{";
		for (size_t i=0; i<subactions.size(); i++)
		{
			if (i!=0) result+=",";
			if (i==current_subaction) result+="*";
			result+=subactions[i]->str();
		}
		result+="}";
		return result;
	}

	string WalkTo::str() const
	{
		return "WalkTo("+destination.str()+"±"+to_string(allowed_distance)+")";
	}

	string WalkWaypoints::str() const
	{
		return "WalkWaypoints(" + join_string(waypoints, [](Pos p) { return p.str(); }) + ")";
	}

	string MineObject::str() const
	{
		return "MineObject(" + obj.str() + ")";
	}

	string PlaceEntity::str() const
	{
		return "PlaceEntity(" + item->name + "@" + pos.str() + ")";
	}

	string PutToInventory::str() const
	{
		return "PutToInventory(" + to_string(amount) + "x " + item->name + " -> " + entity.str() + ")";
	}
	
	string TakeFromInventory::str() const
	{
		return "TakeFromInventory(" + to_string(amount) + "x " + item->name + " <- " + entity.str() + ")";
	}

	string CraftRecipe::str() const
	{
		return "CraftRecipe(" + to_string(count) + "x " + recipe->name + ")";
	}

	/*
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
				subactions.emplace_back( make_unique<action::WalkAndMineObject>(
					game, player, tree) );
		}
		else if (item == "copper-ore" || item == "iron-ore" || item == "stone" || item == "coal")
		{
			throw runtime_error("not implemented yet"); // FIXME
		}
		else
			assert(false);
	}
	*/
}
