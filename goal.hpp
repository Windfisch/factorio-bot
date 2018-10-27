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

#pragma once

#include "entity.h"
#include "action.hpp"

#include <vector>
#include <optional>
#include <memory>

namespace goal
{

/** Interface for all immediate goals. An immediate goal is a predicate on the "relevant" game state, which consists of
  * the map's state (entities, their settings and inventories) and the players' special inventories, such as the axe.
  *
  * These goals can (and will) be reordered at will.
  */
struct GoalInterface
{
	/** calculates the actions required to fulfill this goal.
	  *
	  * returns the empty list if fulfilled(FactorioGame* game)==true.
	  */
	virtual std::vector<std::shared_ptr<action::ActionBase>> calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const
	{
		if (!fulfilled(game))
			return _calculate_actions(game, player, owner);
		else
			return {};
	}
	
	/** internal helper to calculate the actions required to fulfill this goal.
	  * Its result will be ignored, if fulfilled(FactorioGame* game)==true. */
	virtual std::vector<std::shared_ptr<action::ActionBase>> _calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const = 0;
	virtual bool fulfilled(FactorioGame* game) const = 0;
	virtual std::string str(FactorioGame* game) const
	{
		return (fulfilled(game) ? "[x] " : "[ ] ") + str();
	}
	virtual std::string str() const = 0;
	virtual ~GoalInterface() = default;
};

struct PlaceEntity : public GoalInterface
{
	Entity entity;

	PlaceEntity(Entity e) : entity(e) {}

	virtual std::vector<std::shared_ptr<action::ActionBase>> _calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const;
	virtual bool fulfilled(FactorioGame* game) const;
	std::string str() const;
};

struct RemoveEntity : public GoalInterface
{
	Entity entity;

	RemoveEntity(Entity e) : entity(e) {}

	virtual std::vector<std::shared_ptr<action::ActionBase>> _calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const;
	virtual bool fulfilled(FactorioGame* game) const;
	std::string str() const;
};

/* TODO
struct SetupEntity : public GoalInterface
{
	virtual std::vector<std::shared_ptr<action::ActionBase>> _calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const;
	virtual bool fulfilled(FactorioGame* game) const;
	std::string str() const;
};
*/

struct InventoryPredicate : public GoalInterface
{
	enum type_t { POSITIVE, NEGATIVE };

	/** The entity whose inventory shall be modified */
	Entity entity;

	/** The predicate type: POSITIVE means we want at least the item amounts from the list,
	  * other additional items are fine. NEGATIVE means that we want at most the item amounts
	  * from the list, and no items that aren't contained in the list.
	  *
	  * Example: POSITIVE and {"coal", 100} will refuel a furnace if appropriate, while
	  * NEGATIVE and {} will empty a chest completely. */
	type_t type;
	inventory_t inventory_type;

	/** the meaning of this depends on the type */
	Inventory desired_inventory;


	

	
	InventoryPredicate(Entity ent, Inventory inv, inventory_t invtype, type_t type = POSITIVE) :
		entity(ent), type(POSITIVE), desired_inventory(inv), inventory_type(invtype) {}
	
	virtual std::vector<std::shared_ptr<action::ActionBase>> _calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const;
	virtual bool fulfilled(FactorioGame* game) const;
	std::string str() const;
};


struct GoalList : public std::vector< std::shared_ptr<GoalInterface> >
{
	/** returns a list of actions that will make all goals fulfilled, in arbitrary order.
	
	(Effectively, this might re-order its goals due to travelling-salesman) */
	std::vector<std::shared_ptr<action::ActionBase>> calculate_actions(FactorioGame* game, int player, std::optional<owner_t> owner) const;

	/** returns whether all goals are fulfilled */
	bool all_fulfilled(FactorioGame* game) const;
	void dump() const;
	void dump(FactorioGame* game) const;
};

} // namespace goal
