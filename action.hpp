#pragma once

#include <unordered_set>
#include <iostream>
#include <vector>
#include <memory>
#include <stdexcept>
#include "pos.hpp"
#include "entity.h"
#include "resource.hpp"

class FactorioGame;

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
		virtual void on_mined_item(std::string type, int count) {}
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
			for (auto& subgoal : subgoals)
				subgoal->on_mined_item(type, count);
		}
	};

	struct WalkTo : public CompoundGoal
	{
		Pos destination;

		WalkTo(FactorioGame* game, int player, Pos destination_) : CompoundGoal(game, player) { destination = destination_; }
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

	struct WalkAndMineResource : public CompoundGoal
	{
		std::shared_ptr<ResourcePatch> patch;
		int amount;

		WalkAndMineResource(FactorioGame* game, int player, std::shared_ptr<ResourcePatch> patch_, int amount_)
			: CompoundGoal(game,player), patch(patch_), amount(amount_) { assert(amount>0); }

		void start();
		void tick();
		void on_mined_item(std::string type, int amount);
	};
} // namespace action
