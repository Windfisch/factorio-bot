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

#include <unordered_set>
#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include <boost/container/flat_map.hpp>
#include "pos.hpp"
#include "entity.h"
#include "resource.hpp"
#include "unused.h"
#include "defines.h"
#include "item.h"
#include "clock.hpp"

class FactorioGame;
namespace sched { struct Task; }

namespace action
{
	struct PrimitiveAction;

	struct Registry : public std::unordered_map<int, std::weak_ptr<PrimitiveAction>>
	{
		void cleanup();
		std::shared_ptr<PrimitiveAction> get(int id);
	};
	extern Registry registry; // FIXME this should be in FactorioGame

	struct ActionBase
	{
		virtual bool is_finished() const = 0;
		virtual void start() = 0;
		virtual void tick() = 0;
		virtual void abort() {}
		virtual void on_mined_item(std::string type, int count) { UNUSED(type); UNUSED(count); }
		virtual ~ActionBase() = default;
		virtual std::string str() const = 0;

		virtual std::optional<Pos> first_pos() const { return std::nullopt; }

		/** returns the position of the player after having executed this action, and the time required
		  * for doing so, under the assumption that we're at current_position now. */
		virtual std::pair<Pos, Clock::duration> walk_result(Pos current_position) const = 0;

		/** Inventory balance after having fully executed this action.
		  * returns a map of (item, balance) pairs, where a positive balance signifies
		  * receiving this item, negative means consuming the item */
		virtual item_balance_t inventory_balance() const = 0;

		/** Inventory balance when launching this action. E.g. for CraftAction,
		  * this returns negative balance for the ingredients, but does not report
		  * the products yet (because they won't be added immediately at launch)
		  *
		  * `tick` needs to be the timestamp of the inventory to extrapolate. */
		virtual item_balance_t extrapolate_inventory(int tick) const = 0;

		/** returns a (position, radius) pair. The player must be within `radius`
		  * around `position` for the action to be executed.
		  *
		  * if radius is negative, then there is no such requirement. */
		//[[deprecated]] virtual zone_t zone() const = 0;

	};

	struct CompoundAction : public ActionBase
	{
		std::vector< std::unique_ptr<ActionBase> > subactions;
		size_t current_subaction = 0;

		CompoundAction() {}
		
		std::string str() const;

		std::optional<Pos> first_pos() const
		{
			for (const auto& action : subactions)
			{
				if (std::optional<Pos> pos = action->first_pos(); pos.has_value())
					return pos;
			}
			return std::nullopt;
		}
		
		void tick()
		{
			if (!is_finished())
			{
				subactions[current_subaction]->tick();

				if (subactions[current_subaction]->is_finished())
				{
					current_subaction++;

					if (!is_finished())
						subactions[current_subaction]->start();
				}
			}
		}

		bool is_finished() const
		{
			return subactions.size() == current_subaction;
		}

		void start()
		{
			if (!subactions.empty())
				subactions[0]->start();
		}

		// dispatch events
		void on_mined_item(std::string type, int count)
		{
			// FIXME shouldn't this be subaction[current_subaction] only?
			for (auto& subaction : subactions)
				subaction->on_mined_item(type, count);
		}
		
		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const
		{
			Pos pos = current_position;
			Clock::duration sum(0);
			for (const auto& action : subactions)
			{
				auto [pos_after, duration] = action->walk_result(pos);
				assert(duration.count() >= 0);
				pos = pos_after;
				sum += duration;
				assert(sum.count() >= 0);
			}
			return std::pair(pos,sum);
		}

		item_balance_t inventory_balance() const;
		item_balance_t extrapolate_inventory(int tick) const
		{
			if (is_finished())
				return {};
			else
				return subactions[current_subaction]->extrapolate_inventory(tick);
		}
	};

	struct WalkTo : public CompoundAction
	{
		Pos destination;
		double allowed_distance;

		FactorioGame* game;
		int player;
		
		std::string str() const;

		std::optional<Pos> first_pos() const
		{
			return destination;
		}

		WalkTo(FactorioGame* game_, int player_, Pos destination_, double allowed_distance_ = 1.) { game = game_; player = player_; destination = destination_; allowed_distance = allowed_distance_; }
		void start();
		
		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const; // TODO FIXME implement
	};

	// TODO: primitive action list

	struct PrimitiveAction : public ActionBase
	{
		int player;
		FactorioGame* game;
		std::optional<owner_t> owner;
		
		static int id_counter;
		bool finished = false;
		
		int id;
		PrimitiveAction(FactorioGame* game_, int player_, std::optional<owner_t> owner_) { game = game_; player = player_; id = id_counter++; owner = owner_;}

		bool is_finished() const
		{
			return finished;
		}

		void start();

		void tick() {}
		
		int confirmed_tick = 0; // TODO FIXME set properly!

		virtual item_balance_t inventory_balance_on_launch() const = 0;

		item_balance_t extrapolate_inventory(int tick) const
		{
			if (confirmed_tick && tick >= confirmed_tick)
				return {};
			else
				return inventory_balance_on_launch();
		}

		private: virtual void execute_impl() = 0;
	};

	struct WalkWaypoints : public PrimitiveAction
	{
		WalkWaypoints(FactorioGame* game, int player, std::optional<owner_t> owner, const std::vector<Pos>& waypoints_) : PrimitiveAction(game,player,owner), waypoints(waypoints_) {}
		std::vector<Pos> waypoints;
		
		std::string str() const;

		std::optional<Pos> first_pos() const
		{
			if (waypoints.empty()) return std::nullopt;
			else return waypoints[0];
		}
		
		item_balance_t inventory_balance() const { return {}; }
		item_balance_t inventory_balance_on_launch() const { return {}; }
		
		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const;

		private: void execute_impl();
	};
	
	struct MineObject : public PrimitiveAction // TODO amount (default: 0, means "mine it until gone")
	{
		MineObject(FactorioGame* game, int player, std::optional<owner_t> owner, Entity obj_) : PrimitiveAction(game,player,owner), obj(obj_) {}
		Entity obj;
		
		std::string str() const;

		item_balance_t inventory_balance() const;
		item_balance_t inventory_balance_on_launch() const { return {}; }

		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const
		{
			return std::pair(current_position, std::chrono::seconds(1));
		}

		private: void execute_impl();
		private: void abort();
	};

	struct PlaceEntity : public PrimitiveAction
	{
		PlaceEntity(FactorioGame* game, int player, std::optional<owner_t> owner, const ItemPrototype* item_, Pos_f pos_, dir4_t direction_=NORTH) : PrimitiveAction(game,player,owner), item(item_), pos(pos_), direction(direction_) {}
		const ItemPrototype* item;
		Pos_f pos;
		dir4_t direction;
		
		std::string str() const;

		item_balance_t inventory_balance() const { return {{item, -1}}; }
		item_balance_t inventory_balance_on_launch() const { return inventory_balance(); }

		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const
		{
			return std::pair(current_position, std::chrono::seconds(1));
		}

		private: void execute_impl();
	};

	struct PutToInventory : public PrimitiveAction
	{
		const ItemPrototype* item;
		int amount;
		Entity entity;
		inventory_t inventory_type;

		std::string str() const;

		PutToInventory(FactorioGame* game, int player, std::optional<owner_t> owner, const ItemPrototype* item_, int amount_, Entity entity_, inventory_t inventory_type_) :
			PrimitiveAction(game,player,owner), item(item_), amount(amount_), entity(entity_), inventory_type(inventory_type_) { }

		item_balance_t inventory_balance() const { return {{item, -amount}}; }
		item_balance_t inventory_balance_on_launch() const { return inventory_balance(); }

		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const
		{
			return std::pair(current_position, std::chrono::seconds(1));
		}
		private: void execute_impl();
	};

	struct TakeFromInventory : public PrimitiveAction
	{ 
		const ItemPrototype* item;
		int amount;
		Entity entity;
		inventory_t inventory_type;

		TakeFromInventory(FactorioGame* game, int player, std::optional<owner_t> owner, const ItemPrototype* item_, int amount_, Entity entity_, inventory_t inventory_type_) :
			PrimitiveAction(game,player,owner), item(item_), amount(amount_), entity(entity_), inventory_type(inventory_type_) { }

		std::string str() const;

		item_balance_t inventory_balance() const { return {{item, amount}}; }
		item_balance_t inventory_balance_on_launch() const { return inventory_balance(); }
		
		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const
		{
			return std::pair(current_position, std::chrono::seconds(1));
		}
		private: void execute_impl();
	};

	struct CraftRecipe : public PrimitiveAction
	{
		CraftRecipe(FactorioGame* game, int player, std::optional<owner_t> owner, const Recipe* recipe_, int count_=1) : PrimitiveAction(game,player,owner), recipe(recipe_), count(count_) {}

		const Recipe* recipe;
		int count;
		
		std::string str() const;

		item_balance_t inventory_balance() const { throw std::runtime_error("not implemented"); }
		item_balance_t inventory_balance_on_launch() const;

		std::pair<Pos, Clock::duration> walk_result(Pos) const
		{
			throw std::runtime_error("not implemented");
		}
		private: void execute_impl();
	};
} // namespace action
