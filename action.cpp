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

	void Registry::start_action(const std::shared_ptr<ActionBase>& action)
	{
		if (auto primitive_action = std::dynamic_pointer_cast<PrimitiveAction>(action))
		{
			cout << "inserting action #" << primitive_action->id << " into the registry" << endl;
			(*this)[primitive_action->id] = primitive_action;
		}
		action->start();
	}

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

	shared_ptr<PrimitiveAction> Registry::get(int id)
	{
		auto iter = find(id);
		if (iter == end())
			return nullptr;
		else
			return iter->second.lock();
	}

	int PrimitiveAction::id_counter;

	void PrimitiveAction::start()
	{
		execute_impl();
		game->players[player].inventory.apply(inventory_balance_on_launch(), owner);
		cout << "Launched action " << str() << ". Extrapolated inventory is now" << endl;
		game->players[player].inventory.dump();
	}

	void WalkTo::start()
	{
		std::vector<Pos> waypoints = a_star(game->players[player].position.to_int(), destination, game->walk_map, allowed_distance);
		subactions.push_back(unique_ptr<ActionBase>(new WalkWaypoints(game,player,nullopt, waypoints)));

		registry.start_action(subactions[0]);
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
		finished = true;
	}

	void CraftRecipe::execute_impl()
	{
		game->start_crafting(id, player, recipe->name, count);
	}

	void PlaceEntity::execute_impl()
	{
		game->place_entity(player, item->name, pos, direction);
		finished = true;
	}

	void PutToInventory::execute_impl()
	{
		game->insert_to_inventory(player, item->name, amount, entity, inventory_type);
		finished = true;
	}

	void TakeFromInventory::execute_impl()
	{
		game->remove_from_inventory(player, item->name, amount, entity, inventory_type);
		finished = true;
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

	item_balance_t CraftRecipe::inventory_balance_on_launch() const
	{
		item_balance_t result;
		for (const auto& ingredient : recipe->ingredients)
			result[ingredient.item] -= ingredient.amount;
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
}
