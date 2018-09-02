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
	struct Registry : public std::unordered_map<int, std::weak_ptr<sched::Task>>
	{
		void cleanup();
		std::shared_ptr<sched::Task> get(int id);
	};
	extern Registry registry;

	struct ActionBase
	{
		virtual bool is_finished() = 0;
		virtual void start() = 0;
		virtual void tick() = 0;
		virtual void abort() { throw std::runtime_error("abort() not implemented for this action"); }
		virtual void on_mined_item(std::string type, int count) { UNUSED(type); UNUSED(count); }
		virtual ~ActionBase() = default;
		virtual std::string str() const = 0;

		virtual std::optional<Pos> first_pos() const { return std::nullopt; }

		/** returns the position of the player after having executed this action, and the time required
		  * for doing so, under the assumption that we're at current_position now. */
		virtual std::pair<Pos, Clock::duration> walk_result(Pos current_position) const = 0;

		/** returns a map of (item, balance) pairs, where a positive balance signifies
		  * receiving this item, negative means consuming the item */
		virtual item_balance_t inventory_balance() const = 0;

		/** returns a (position, radius) pair. The player must be within `radius`
		  * around `position` for the action to be executed.
		  *
		  * if radius is negative, then there is no such requirement. */
		//[[deprecated]] virtual zone_t zone() const = 0;

	};

	struct CompoundGoal : public ActionBase
	{
		std::vector< std::unique_ptr<ActionBase> > subgoals;
		size_t current_subgoal = 0;

		CompoundGoal() {}
		
		std::string str() const;

		std::optional<Pos> first_pos() const
		{
			for (const auto& action : subgoals)
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
				subgoals[current_subgoal]->tick();

				if (subgoals[current_subgoal]->is_finished())
				{
					current_subgoal++;

					if (!is_finished())
						subgoals[current_subgoal]->start();
				}
			}
		}

		bool is_finished()
		{
			return subgoals.size() == current_subgoal;
		}

		void start()
		{
			if (!subgoals.empty())
				subgoals[0]->start();
		}

		// dispatch events
		void on_mined_item(std::string type, int count)
		{
			// FIXME shouldn't this be subgoal[current_subgoal] only?
			for (auto& subgoal : subgoals)
				subgoal->on_mined_item(type, count);
		}
		
		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const
		{
			Pos pos = current_position;
			Clock::duration sum;
			for (const auto& action : subgoals)
			{
				auto [pos_after, duration] = action->walk_result(pos);
				pos = pos_after;
				sum += duration;
			}
			return std::pair(pos,sum);
		}

		item_balance_t inventory_balance() const;
	};

	struct WalkTo : public CompoundGoal
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
		
		static int id_counter;
		static std::unordered_set<int> pending;
		static std::unordered_set<int> finished;
		
		static void mark_finished(int id)
		{
			if (!pending.erase(id))
				std::cout << "STRANGE: mark_finished("<<id<<") called, but not in pending set" << std::endl;
			finished.insert(id);
		}
		
		int id;
		PrimitiveAction(FactorioGame* game_, int player_) { game = game_; player = player_; id = id_counter++; }

		bool is_finished()
		{
			return finished.find(id) != finished.end();
		}

		void start()
		{
			pending.insert(id);
			execute_impl();
		}

		void tick() {}

		private: virtual void execute_impl() = 0;
	};

	struct WalkWaypoints : public PrimitiveAction
	{
		WalkWaypoints(FactorioGame* game, int player, const std::vector<Pos>& waypoints_) : PrimitiveAction(game,player), waypoints(waypoints_) {}
		std::vector<Pos> waypoints;
		
		std::string str() const;

		std::optional<Pos> first_pos() const
		{
			if (waypoints.empty()) return std::nullopt;
			else return waypoints[0];
		}
		
		item_balance_t inventory_balance() const { return {}; }
		// [[deprecated("FIXME REMOVE")]] zone_t zone() const { return {Pos_f(), -1.f}; }
		
		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const;

		private: void execute_impl();
	};
	
	struct MineObject : public PrimitiveAction // TODO amount (default: 0, means "mine it until gone")
	{
		MineObject(FactorioGame* game, int player, Entity obj_) : PrimitiveAction(game,player), obj(obj_) {}
		Entity obj;
		
		std::string str() const;

		item_balance_t inventory_balance() const; // TODO
		// [[deprecated("FIXME REMOVE")]] zone_t zone() const { return {obj.pos, REACH}; }

		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const
		{
			return std::pair(current_position, std::chrono::seconds(1));
		}

		private: void execute_impl();
		private: void abort();
	};

	struct PlaceEntity : public PrimitiveAction
	{
		PlaceEntity(FactorioGame* game, int player, const ItemPrototype* item_, Pos_f pos_, dir4_t direction_=NORTH) : PrimitiveAction(game,player), item(item_), pos(pos_), direction(direction_) {}
		const ItemPrototype* item;
		Pos_f pos;
		dir4_t direction;
		
		std::string str() const;

		item_balance_t inventory_balance() const { return {{item, -1}}; }
		// [[deprecated("FIXME REMOVE")]] zone_t zone() const { return {pos, REACH}; }

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

		PutToInventory(FactorioGame* game, int player, const ItemPrototype* item_, int amount_, Entity entity_, inventory_t inventory_type_) :
			PrimitiveAction(game,player), item(item_), amount(amount_), entity(entity_), inventory_type(inventory_type_) { }

		item_balance_t inventory_balance() const { return {{item, -amount}}; }
		// [[deprecated("FIXME REMOVE")]] zone_t zone() const { return {entity.pos, REACH}; }

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

		TakeFromInventory(FactorioGame* game, int player, const ItemPrototype* item_, int amount_, Entity entity_, inventory_t inventory_type_) :
			PrimitiveAction(game,player), item(item_), amount(amount_), entity(entity_), inventory_type(inventory_type_) { }

		std::string str() const;

		item_balance_t inventory_balance() const { return {{item, amount}}; }
		// [[deprecated("FIXME REMOVE")]] zone_t zone() const { return {entity.pos, REACH}; }
		
		std::pair<Pos, Clock::duration> walk_result(Pos current_position) const
		{
			return std::pair(current_position, std::chrono::seconds(1));
		}
		private: void execute_impl();
	};

	struct [[deprecated]] CraftRecipe : public PrimitiveAction
	{
		CraftRecipe(FactorioGame* game, int player, const Recipe* recipe_, int count_=1) : PrimitiveAction(game,player), recipe(recipe_), count(count_) {}

		const Recipe* recipe;
		int count;
		
		std::string str() const;

		item_balance_t inventory_balance() const { throw std::runtime_error("not implemented"); }
		// [[deprecated("FIXME REMOVE")]] zone_t zone() const { return {Pos_f(), -1.f}; }

		std::pair<Pos, Clock::duration> walk_result(Pos) const
		{
			throw std::runtime_error("not implemented");
		}
		private: void execute_impl();
	};
} // namespace action
