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
#include "pos.hpp"
#include "entity.h"
#include "resource.hpp"
#include "unused.h"
#include "defines.h"

class FactorioGame;
namespace sched { struct Task; }

namespace action
{
	struct PlayerGoal
	{
		int player;
		FactorioGame* game;
		
		PlayerGoal(FactorioGame* g, int player_) { game = g; player = player_; }
		virtual bool is_finished() = 0;
		virtual void start() = 0;
		virtual void tick() = 0;
		virtual void abort() { throw std::runtime_error("abort() not implemented for this action"); }
		virtual void on_mined_item(std::string type, int count) { UNUSED(type); UNUSED(count); }
		virtual ~PlayerGoal() = default;
	};

	struct CompoundGoal : public PlayerGoal
	{
		std::vector< std::unique_ptr<PlayerGoal> > subgoals;
		size_t current_subgoal = 0;

		CompoundGoal(FactorioGame* game, int player) : PlayerGoal(game, player) {}

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
	};

	struct ParallelGoal : public PlayerGoal
	{
		std::vector< std::unique_ptr<PlayerGoal> > subgoals;

		ParallelGoal(FactorioGame* game, int player) : PlayerGoal(game, player) {}

		void tick()
		{
			for (auto& subgoal : subgoals)
				if (!subgoal->is_finished())
					subgoal->tick();
		}

		void start()
		{
			for (auto& subgoal : subgoals)
				subgoal->start();
		}

		bool is_finished()
		{
			bool all_done = true;
			for (auto& subgoal : subgoals)
				all_done = all_done && subgoal->is_finished();
			return all_done;
		}
		
		// dispatch events
		void on_mined_item(std::string type, int count)
		{
			for (auto& subgoal : subgoals)
				subgoal->on_mined_item(type, count);
		}
	};

	struct WalkTo : public CompoundGoal
	{
		Pos destination;
		double allowed_distance;

		WalkTo(FactorioGame* game, int player, Pos destination_, double allowed_distance_ = 0.) : CompoundGoal(game, player) { destination = destination_; allowed_distance = allowed_distance_; }
		void start();
	};

	struct PrimitiveAction : public PlayerGoal
	{
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
		PrimitiveAction(FactorioGame* game, int player) : PlayerGoal(game, player) { id = id_counter++; }

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

		private: void execute_impl();
	};
	
	struct MineObject : public PrimitiveAction
	{
		MineObject(FactorioGame* game, int player, Entity obj_) : PrimitiveAction(game,player), obj(obj_) {}
		Entity obj;

		private: void execute_impl();
		private: void abort();
	};

	struct PlaceEntity : public PrimitiveAction
	{
		PlaceEntity(FactorioGame* game, int player, std::string item_, Pos_f pos_, dir8_t direction_=d8_NORTH) : PrimitiveAction(game,player), item(item_), pos(pos_), direction(direction_) {}
		std::string item;
		Pos_f pos;
		dir8_t direction;

		private: void execute_impl();
	};

	struct WalkAndPlaceEntity : public CompoundGoal
	{
		WalkAndPlaceEntity(FactorioGame* game, int player, std::string item_, Pos_f pos_, dir8_t direction_=d8_NORTH) : CompoundGoal(game,player), item(item_), pos(pos_), direction(direction_) {}
		std::string item;
		Pos_f pos;
		dir8_t direction;
		
		void start();
	};

	struct WalkAndMineObject : public CompoundGoal
	{
		WalkAndMineObject(FactorioGame* game, int player, Entity obj_) : CompoundGoal(game,player), obj(obj_) {}
		Entity obj;

		void start();
	};

	struct WalkAndMineResource : public CompoundGoal
	{
		std::shared_ptr<ResourcePatch> patch;
		int amount;

		WalkAndMineResource(FactorioGame* game, int player, std::shared_ptr<ResourcePatch> patch_, int amount_)
			: CompoundGoal(game,player), patch(patch_), amount(amount_)
		{
			if (amount <= 0) throw std::invalid_argument("amount must be positive");
		}

		void start();
		void tick();
		void on_mined_item(std::string type, int amount);
	};

	/** Should take whatever action deemed appropriate to have `amount` of `item` in the inventory. Possible
	  * ways to accomplish this include mining the desired amount, fetching the amount from a chest or handcrafting
	  * the items. This may or may not change the player's position */
	struct HaveItem : public CompoundGoal
	{
		std::string item;
		int amount;
		
		HaveItem(FactorioGame* game, int player, std::string item_, int amount_)
			: CompoundGoal(game,player), item(item_), amount(amount_)
		{
			if (amount <= 0) throw std::invalid_argument("amount must be positive");
			if (item != "raw-wood" && item != "copper-ore" && item != "iron-ore" && item != "stone" && item != "coal") throw std::invalid_argument("unsupported item type for now");
		}

		void start();
	};

	struct PutToInventory : public PrimitiveAction
	{
		std::string item;
		int amount;
		Entity entity;
		inventory_t inventory_type;

		PutToInventory(FactorioGame* game, int player, std::string item_, int amount_, Entity entity_, inventory_t inventory_type_) :
			PrimitiveAction(game,player), item(item_), amount(amount_), entity(entity_), inventory_type(inventory_type_) { }

		private: void execute_impl();
	};

	struct TakeFromInventory : public PrimitiveAction
	{
		std::string item;
		int amount;
		Entity entity;
		inventory_t inventory_type;

		TakeFromInventory(FactorioGame* game, int player, std::string item_, int amount_, Entity entity_, inventory_t inventory_type_) :
			PrimitiveAction(game,player), item(item_), amount(amount_), entity(entity_), inventory_type(inventory_type_) { }

		private: void execute_impl();
	};

	struct CraftRecipe : public PrimitiveAction
	{
		CraftRecipe(FactorioGame* game, int player, std::string recipe_, int count_=1) : PrimitiveAction(game,player), recipe(recipe_), count(count_) {}

		std::string recipe;
		int count;

		private: void execute_impl();
	};
} // namespace action
