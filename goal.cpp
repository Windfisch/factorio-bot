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

#include "goal.hpp"
#include "factorio_io.h"
#include "util.hpp"

#include <boost/range/iterator_range_core.hpp>

using boost::make_iterator_range;

using namespace std;

const int RESOURCE_REACH = 2; // max distance for mining a tree, ore field or simple-entity
const int REACH = 4; // max distance for mining a player-built entity // FIXME magic constant. move to common header
const int SAFETY_DISTANCE = 2; // minimum distance for building a player-built entity

static_assert(REACH > SAFETY_DISTANCE);

namespace goal
{

bool GoalList::all_fulfilled(FactorioGame* game) const
{
	for (const auto& goal : (*this))
		if (!goal->fulfilled(game))
			return false;

	return true;
}

vector<shared_ptr<action::ActionBase>> GoalList::calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const
{
	vector<shared_ptr<action::ActionBase>> result;

	for (const auto& goal : (*this))
	{
		auto actions = goal->calculate_actions(game, player, owner);
		result.insert(std::end(result),
				std::make_move_iterator(std::begin(actions)),
				std::make_move_iterator(std::end(actions)));
	}

	return result;
}

bool PlaceEntity::fulfilled(FactorioGame* game) const
{
	return game->actual_entities.search_or_null(entity) != nullptr;
}
vector<shared_ptr<action::ActionBase>> PlaceEntity::_calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const
{
	const float CLEAR_MARGIN = 0.1f; // FIXME magic number
	vector<shared_ptr<action::ActionBase>> result;

	// clear the area from trees and rocks first
	Area_f area_to_clear = entity.collision_box().expand(CLEAR_MARGIN);
	for (const Entity& offending_entity : game->actual_entities.overlap_range(area_to_clear))
		if (offending_entity.proto->type == "tree" || offending_entity.proto->type == "simple-entity")
		{
			result.push_back( make_unique<action::WalkTo>(game, player, offending_entity.pos, RESOURCE_REACH) );
			result.push_back( make_unique<action::MineObject>(game, player, nullopt, offending_entity) );
		}
	
	// TODO FIXME: ensure that the player isn't in the way.
	result.push_back( make_unique<action::WalkTo>(game, player, entity.collision_box(), REACH, SAFETY_DISTANCE) );
	result.push_back( make_unique<action::PlaceEntity>(game, player, owner, game->get_item_for(entity.proto), entity) );
	return result;
}

bool RemoveEntity::fulfilled(FactorioGame* game) const
{
	return game->actual_entities.search_or_null(entity) == nullptr;
}
vector<shared_ptr<action::ActionBase>> RemoveEntity::_calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const
{
	vector<shared_ptr<action::ActionBase>> result;

	bool is_resource = entity.proto->type == "tree" || entity.proto->type == "simple-entity" || entity.proto->type == "resource";

	result.push_back( make_unique<action::WalkTo>(game, player, entity.pos, is_resource ? RESOURCE_REACH : REACH) );
	result.push_back( make_unique<action::MineObject>(game, player, owner, entity) );
	return result;
}

bool InventoryPredicate::fulfilled(FactorioGame* game) const
{
	Entity* ent = game->actual_entities.search_or_null(entity);
	const MultiInventory& actual_inventory = ent ? ent->data<ContainerData>().inventories : MultiInventory();
	if (type == POSITIVE)
	{
		for (auto [item, amount] : desired_inventory)
			if (actual_inventory.get_or(item, inventory_type, 0) < amount)
				return false;
		return true;
	}
	else
	{
		for (const auto& [key, amount] : make_iterator_range(actual_inventory.inv_begin(inventory_type), actual_inventory.inv_end(inventory_type)))
		{
			if (get_or(desired_inventory, key.item) > amount)
				return false;
		}
		return true;
	}
}
vector<shared_ptr<action::ActionBase>> InventoryPredicate::_calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const
{
	vector<shared_ptr<action::ActionBase>> result;
	result.push_back( make_unique<action::WalkTo>(game, player, entity.pos) );

	Entity* ent = game->actual_entities.search_or_null(entity);
	const MultiInventory& actual_inventory = ent ? ent->data<ContainerData>().inventories : MultiInventory();

	if (type == POSITIVE)
	{
		for (auto [item, desired_amount] : desired_inventory)
		{
			auto actual_amount = actual_inventory.get_or(item, inventory_type, 0);
			if (actual_amount < desired_amount)
				result.push_back(make_unique<action::PutToInventory>(game, player, owner, item, desired_amount-actual_amount, entity, inventory_type));
		}
	}
	else
	{
		for (const auto& [key, actual_amount] : make_iterator_range(actual_inventory.inv_begin(inventory_type), actual_inventory.inv_end(inventory_type)))
		{
			auto desired_amount = get_or(desired_inventory, key.item);
			if (actual_amount > desired_amount)
				result.push_back(make_unique<action::TakeFromInventory>(game, player, owner, key.item, actual_amount-desired_amount, *ent, inventory_type));
		}
	}

	return result;
}

string PlaceEntity::str() const
{
	return "PlaceEntity(" + entity.str() + ")";
}

string RemoveEntity::str() const
{
	return "RemoveEntity(" + entity.str() + ")";
}

string InventoryPredicate::str() const
{
	string result = "InventoryPredicate(" + entity.str() + "[" + inventory_names[inventory_type] + "] -> at ";
	result += (type == POSITIVE ? "least " : "most ");
	result += desired_inventory.str() + ")";
	return result;
}

void GoalList::dump() const
{
	cout << "GoalList:" << endl;
	for (const auto& goal : (*this))
		cout << "\t" << goal->str() << endl;
}
void GoalList::dump(FactorioGame* game) const
{
	cout << "GoalList:" << endl;
	for (const auto& goal : (*this))
		cout << "\t" << goal->str(game) << endl;
}

}
