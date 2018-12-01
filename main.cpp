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

#include <iostream>
#include <climits>
#include <cassert>
#include "factorio_io.h"
#include "scheduler.hpp"
#include "gui/gui.h"
#include "goal.hpp"
#include "mine_planning.h"
#include "logging.hpp"

using namespace std;
using namespace sched;

static float cluster_quality(int diam, size_t coal_size, size_t iron_size, size_t copper_size, size_t stone_size)
{
	const float COAL_SIZE = 100;
	const float IRON_SIZE = 25*10;
	const float COPPER_SIZE = 25*10;
	const float STONE_SIZE = 100;
	return (diam + 50)
		/ min(1.f,coal_size/COAL_SIZE) // penalize too-small chunks
		/ min(1.f,iron_size/IRON_SIZE)
		/ min(1.f,copper_size/COPPER_SIZE)
		/ min(1.f,stone_size/STONE_SIZE);
}

struct start_mines_t
{
	std::shared_ptr<ResourcePatch> coal;
	std::shared_ptr<ResourcePatch> iron;
	std::shared_ptr<ResourcePatch> copper;
	std::shared_ptr<ResourcePatch> stone;
	Pos water;
	Area area;
};

static start_mines_t find_start_mines(FactorioGame* game, GUI::MapGui* gui, Pos_f pos = Pos_f(0.,0.))
{
	Logger log("find_start_mines");
	start_mines_t result;

	// FIXME: we treat "known && !can_walk" as water and "known && can_walk" as land. this is suboptimal.
	// FIXME so many magic numbers!

	// enumerate all relevant resource patches, limit the number to 5

	vector< pair<float, shared_ptr<ResourcePatch>> > patches[Resource::N_RESOURCES];
	for (auto patch : game->resource_patches) if (patch->size() >= 30)
	{
		switch (patch->type)
		{
			case Resource::COAL:
			case Resource::IRON:
			case Resource::COPPER:
			case Resource::STONE:
				patches[patch->type].emplace_back( (pos-patch->bounding_box.center()).len(), patch );
				break;
			
			case Resource::NONE:
				// this should never happen
				assert(patch->type != Resource::NONE);
				break;

			default: // empty
				break;
		}
	}

	// only consider the 5 closest patches of each resource
	int radius = 0;
	for (auto i : {Resource::COAL, Resource::IRON, Resource::COPPER, Resource::STONE})
	{
		sort(patches[i].begin(), patches[i].end());
		patches[i].resize(min(patches[i].size(), size_t(5)));

		if (!patches[i].empty())
			radius = max(radius, int(patches[i].back().first));
		else
			throw runtime_error("find_start_mines could not find "+Resource::typestr[i]);
	}


	// find potential water sources in the starting area
	int water_radius = radius + 100;
	auto view = game->walk_map.view(pos-Pos(water_radius+1,water_radius+1), pos+Pos(water_radius+1,water_radius+1), Pos(0,0));
	auto resview = game->resource_map.view(pos-Pos(water_radius+1,water_radius+1), pos+Pos(water_radius+1,water_radius+1), Pos(0,0));
	struct watersource_t { Pos pos; Pos get_pos() const { return pos;} };
	WorldList<watersource_t> watersources;
	for (int x = int(pos.x)-water_radius; x<=int(pos.x)+water_radius; x++)
		for (int y = int(pos.y)-water_radius; y<=int(pos.y)+water_radius; y++)
			if (view.at(x,y).land())
			{
				int n_water = 0;
				size_t water_size = 0;
				for (Pos p : directions4)
					if (view.at(Pos(x,y)+p).water())
					{
						n_water++;
						water_size = resview.at(Pos(x,y)+p).resource_patch.lock()->size();
					}

				if (n_water == 1 && water_size > 100)
					watersources.insert( watersource_t{Pos(x,y)} );
			}
	
	

	float best = 999999999999; // best quality found so far

	// brute-force all 4-tuples and find the best
	for (auto& coal : patches[Resource::COAL])
		for (auto& iron : patches[Resource::IRON])
			for (auto& copper : patches[Resource::COPPER])
				for (auto& stone : patches[Resource::STONE])
				{
					log << endl;
					log << "coal:\t" << coal.second->bounding_box.center().str() << endl;
					log << "iron:\t" << iron.second->bounding_box.center().str() << endl;
					log << "copper:\t" << copper.second->bounding_box.center().str() << endl;
					log << "stone:\t" << stone.second->bounding_box.center().str() << endl;

					// calculate the bounding box enclosing the 4-tuple of essential resources
					int x1 = min( coal  .second->bounding_box.center().x,
					         min( iron  .second->bounding_box.center().x,
					         min( stone .second->bounding_box.center().x,
					              copper.second->bounding_box.center().x )));
					int x2 = max( coal  .second->bounding_box.center().x,
					         max( iron  .second->bounding_box.center().x,
					         max( stone .second->bounding_box.center().x,
					              copper.second->bounding_box.center().x )));
					int y1 = min( coal  .second->bounding_box.center().y,
					         min( iron  .second->bounding_box.center().y,
					         min( stone .second->bounding_box.center().y,
					              copper.second->bounding_box.center().y )));
					int y2 = max( coal  .second->bounding_box.center().y,
					         max( iron  .second->bounding_box.center().y,
					         max( stone .second->bounding_box.center().y,
					              copper.second->bounding_box.center().y )));

					int diam = max( x2-x1, y2-y1 );
					if (cluster_quality(diam, coal.second->size(), iron.second->size(), copper.second->size(), stone.second->size()) > best)
						continue; // we're worse than the best even without extending the diameter to include water. discard and continue.

					log << "diam = " << diam << endl;

					// find the closest water source and calculate the new bounding box
					Pos_f box_center = Pos_f((x1+x2)/2., (y1+y2)/2.);
					Pos best_water_pos;
					int best_water_diam = INT_MAX;
					for (auto water_pos : watersources.around(box_center))
					{
						if ((water_pos.pos - box_center).len() >= 1.5*(best_water_diam-diam/2) )
							break;

						log << ".";
						
						int new_diam = max(
							max(x2,water_pos.pos.x)-min(x1,water_pos.pos.x),
							max(y2,water_pos.pos.y)-min(y1,water_pos.pos.y) );

						if (new_diam < best_water_diam)
						{
							best_water_diam = new_diam;
							best_water_pos = water_pos.pos;
							log << "\b*";
						}
					}
					log << endl;
					log << "found water at " << best_water_pos.str() << endl;
					log << "best_water_diam = " << best_water_diam << endl;

					x1 = min(x1, best_water_pos.x);
					y1 = min(y1, best_water_pos.y);
					x2 = max(x2, best_water_pos.x);
					y2 = max(y2, best_water_pos.y);

					float quality = cluster_quality(best_water_diam, coal.second->size(), iron.second->size(), copper.second->size(), stone.second->size());
					if (quality < best)
					{
						best=quality;
						// TODO
						result.coal = coal.second;
						result.copper = copper.second;
						result.stone = stone.second;
						result.iron = iron.second;
						result.water = best_water_pos;
						result.area = Area(x1,y1,x2,y2);
					}
				}
	
	gui->rect( result.area.left_top, result.area.right_bottom, GUI::Color(0,255,0) );
	gui->rect( result.coal->bounding_box.left_top, result.coal->bounding_box.right_bottom, GUI::Color(255,0,255) );
	gui->rect( result.copper->bounding_box.left_top, result.copper->bounding_box.right_bottom, GUI::Color(255,0,255) );
	gui->rect( result.iron->bounding_box.left_top, result.iron->bounding_box.right_bottom, GUI::Color(255,0,255) );
	gui->rect( result.stone->bounding_box.left_top, result.stone->bounding_box.right_bottom, GUI::Color(255,0,255) );
	return result;
}


struct debug_draw_actions_state_t
{
	int count = 0;
	Pos last;
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
static void debug_draw_actions(const action::ActionBase* action, GUI::MapGui* gui, debug_draw_actions_state_t& state)
{
	Logger log("debug_draw");
	using namespace action;
	if (auto g = dynamic_cast<const WalkTo*>(action))
	{
		log << "WalkTo " << g->destination.str() << endl;
		GUI::Color color = GUI::Color(255,0,255);
		color.blend( GUI::Color(127,127,127), (state.count%5)/4.0f);

		gui->line(state.last, g->destination.center(), color);
		gui->rect(g->destination.left_top, g->destination.right_bottom, GUI::Color(255,127,0));

		state.last = g->destination.center();
		state.count++;
	}
	else if (auto g = dynamic_cast<const TakeFromInventory*>(action))
	{
		log << "TakeFromInventory" << endl;
		gui->rect(state.last,1, GUI::Color(0,127,255));
	}
	else if (auto g = dynamic_cast<const CompoundAction*>(action))
	{
		log << "recursing into CompoundAction" << endl;
		for (const auto& sub : g->subactions)
			debug_draw_actions(sub.get(), gui, state);
	}
}
#pragma GCC diagnostic pop

static void debug_draw_actions(const action::ActionBase* action, GUI::MapGui* gui, Pos start)
{
	debug_draw_actions_state_t state;
	state.last = start;
	debug_draw_actions(action, gui, state);
}


int main(int argc, const char** argv)
{
	if (argc != 3 && argc != 6)
	{
		cout << "Usage: " << argv[0] << " outfile-prefix datapath [rcon-host rcon-port rcon-password]" << endl;
		cout << "       outfile-prefix: path to the file generated by the mod, minus the '...1.txt' part" << endl;
		cout << "       The rcon options can be left out. In this case, the bot is observe-only." << endl;
		cout << "       This can be useful for debugging, since Factorio doesn't even need to run. It is" << endl;
		cout << "       sufficient to read a previously generated outfile. " << endl;
		return 1;
	}

	FactorioGame factorio(argv[1]);

	cout << "reading static data" << endl;
	while(true)
	{
		string packet = factorio.read_packet();
		if (packet == "0 STATIC_DATA_END")
			break;
		else
			factorio.parse_packet(packet);
	}
	cout << "done reading static data" << endl;

	goal::GoalList test_goals;
	{
		test_goals.push_back( make_unique<goal::PlaceEntity>(Entity(Pos(1,1), &factorio.get_entity_prototype("assembling-machine-1"))) );
		test_goals.push_back( make_unique<goal::PlaceEntity>(Entity(Pos(-3,4), &factorio.get_entity_prototype("wooden-chest"))) );
		test_goals.push_back( make_unique<goal::RemoveEntity>(Entity(Pos(12,12), &factorio.get_entity_prototype("tree-01"))) );
		auto test_actions = test_goals.calculate_actions(&factorio, 1, nullopt);
		test_goals.dump(&factorio);
		cout << "ActionList" << endl;
		item_balance_t balance;
		Pos pos{0,0};
		for (const auto& a : test_actions)
		{
			auto [newpos, duration] = a->walk_result(pos);
			auto bal = a->inventory_balance();

			cout << "\t" << a->str() << " | walks from " << pos.str() << " to " << newpos.str() << " in " << std::chrono::duration_cast<chrono::milliseconds>(duration).count() << "ms" << endl;
			for (auto [k,v] : bal)
				cout << "\t\t" << k->name << " -> " << v << endl;
			pos = newpos;

			accumulate_map(balance, a->inventory_balance());
		}
		cout << "total item balance for this task:" << endl;
		for (auto [k,v] : balance)
			cout << "\t" << k->name << " -> " << v << endl;
	}

	//exit(0);

	bool online = false;
	if (argc == 6)
	{
		string host(argv[3]);
		int port = atoi(argv[4]);
		string pass(argv[5]);

		factorio.rcon_connect(host, port, pass);
		online = true;
	}

	int frame=0;

	GUI::MapGui gui(&factorio, argv[2]);

	// quick read first part of the file before doing any GUI work. Useful for debugging, since reading in 6000 lines will take more than 6 seconds.
	for (int i=0; i<6000; i++) 
	{
		//cout << i << endl;
		factorio.parse_packet(factorio.read_packet());
	}
	
	size_t player_idx = SIZE_MAX;
	for (size_t i = 0; i < factorio.players.size(); i++)
		if (factorio.players[i].connected)
		{
			player_idx = i;
			break;
		}
	
	if (player_idx == SIZE_MAX)
	{
		cout << "Could not determine a player index :(" << endl;
		if (!online)
		{
			cout << "making up one (1)" << endl;
			player_idx = 1;
		}
		else
			exit(1);
	}
	cout << endl << "Player index = " << player_idx << endl << endl;

	struct StrategyPlayer
	{
		Scheduler scheduler;

		shared_ptr<Task> current_task = nullptr;
		enum class TaskExecutionState { APPROACHING_START_LOCATION, AWAITING_LAUNCH, LAUNCHED, FINISHED };
		TaskExecutionState task_execution_state = TaskExecutionState::FINISHED;

		std::shared_ptr<action::CraftRecipe> current_crafting_action;
		std::optional<Scheduler::owned_recipe_t> current_craft;

		StrategyPlayer(FactorioGame* game, size_t idx) : scheduler(game,idx) {}

		void update_task(FactorioGame* game, Player& player, const std::shared_ptr<Task>& task)
		{
			Logger log("strategy");
			log << "Player " << player.id << "'s current task has changed from "
				<< (current_task ? current_task->name : "(null)")
				<< " to " << (task ? task->name : "(null)") << endl;
			
			current_task = task;
			player.set_actions(nullptr);
			task_execution_state = TaskExecutionState::FINISHED;

			if (task)
			{
				task_execution_state = TaskExecutionState::APPROACHING_START_LOCATION;
				auto approach_action = make_shared<action::WalkTo>(game, player.id, task->start_location, task->start_radius);
				player.set_actions(approach_action);
			}
		}

		void tick_scheduler(FactorioGame* game, Player& player)
		{
			Logger log("strategy");
			std::shared_ptr<Task> task = scheduler.get_current_task();

			if (current_task != task)
				update_task(game, player, task);

			assert(current_task == task);

			bool actions_finished = player.tick_actions();

			switch (task_execution_state)
			{
				case TaskExecutionState::APPROACHING_START_LOCATION:
					if (actions_finished)
					{
						log << "player #" << player.id << " has approached task '" << current_task->name << "''s start location" << endl;
						task_execution_state = TaskExecutionState::AWAITING_LAUNCH;
					}
					else
						break;
					[[fallthrough]];

				case TaskExecutionState::AWAITING_LAUNCH:
					if (player.inventory.can_satisfy(current_task->required_items, current_task->owner_id))
					{
						log << "player #" << player.id << " has launched task '" << current_task->name << "'" << endl;
						player.set_actions(current_task->actions);
						actions_finished = false;
						task_execution_state = TaskExecutionState::LAUNCHED;
					}
					break;

				case TaskExecutionState::LAUNCHED:
					if (actions_finished)
					{
						log << "player #" << player.id << "'s task '" << current_task->name << "' has finished. ";
						if (current_task->goals.has_value())
						{
							log << "Its goals are " << (current_task->goals->all_fulfilled(game) ? "" : "NOT ") << "fulfilled:" << endl;
							current_task->goals->dump(game);
						}
						else
							log << "It had no goals, only actions (which have been executed)" << endl;
						
						log << "removing that task from the scheduler..." << endl;
						scheduler.remove_task(current_task);

						if (current_task->finished_callback)
							current_task->finished_callback();

						current_task = nullptr;
						task_execution_state = TaskExecutionState::FINISHED;

						scheduler.recalculate();
					}
					else
						break;

					[[fallthrough]];

				case TaskExecutionState::FINISHED:
					//scheduler.recalculate();
					break;
			}

			tick_craftinglist();
		}

		void tick_craftinglist()
		{
			Logger log("craftinglist");
			assert( (current_crafting_action!=nullptr) == current_craft.has_value() );
			if (current_crafting_action)
				assert(current_crafting_action->recipe == current_craft->second);

			// check if the current craft has finished
			if (current_craft.has_value())
			{
				if (current_crafting_action->is_finished())
				{
					auto t = current_craft->first.lock();
					log << "player #" << scheduler.player_idx << " has finished crafting " << current_craft->second->name << "(" << (t ? t->name : "?") << ")" << endl;

					scheduler.confirm_current_craft(current_craft.value());
					current_crafting_action = nullptr;
					current_craft = nullopt;

					log << "there is " << (scheduler.peek_current_craft() ? "a" : "no") << " next craft." << endl;
				}
			}

			// check if there is a new craft
			// TODO FIXME: ugly.
			if (current_craft.has_value() != scheduler.peek_current_craft().has_value() || (current_craft.has_value() && scheduler.peek_current_craft().has_value() && !Scheduler::owned_recipe_t_equal(current_craft.value(), scheduler.peek_current_craft().value())))
			{
				log << "player #" << scheduler.player_idx << "'s current_craft has changed from ";
				if (current_craft.has_value())
				{
					log << current_craft->second->name;
					if (auto t = current_craft->first.lock())
						log << "(" << t->name << ")";
					else
						log << "(?)";
				}
				else
					log << "(null)";

				log << " to ";
				
				if (scheduler.peek_current_craft().has_value())
				{
					log << scheduler.peek_current_craft()->second->name;
					if (auto t = scheduler.peek_current_craft()->first.lock())
						log << "(" << t->name << ")";
					else
						log << "(?)";
				}
				else
					log << "(null)";

				log << endl;

				
				// abort the previous craft
				if (current_craft.has_value())
				{
					log << "aborting previous craft" << endl;
					current_crafting_action->abort();
					scheduler.retreat_current_craft(current_craft.value());
					current_crafting_action = nullptr;
					current_craft = nullopt;
				}

				current_craft = scheduler.peek_current_craft();

				// launch the new
				if (current_craft.has_value())
				{
					auto owning_task = std::shared_ptr(current_craft->first);
					// create and launch a CraftRecipe action
					current_crafting_action = make_shared<action::CraftRecipe>(scheduler.game, scheduler.player_idx, owning_task->owner_id, current_craft->second);
					owning_task->associated_crafting_actions.push_back(current_crafting_action);
					action::registry.start_action(current_crafting_action);
					// confirm this to the scheduler
					scheduler.accept_current_craft();
				}
			}

		}
	};

	vector<StrategyPlayer> splayers;
	for (size_t i=0; i<=player_idx; i++)
		splayers.emplace_back(&factorio, i);

	// create some initial tasks
	{
		auto mytask = make_shared<Task>("axe crafter");
		mytask->goals.reset();
		mytask->required_items.emplace_back(ItemStack{&factorio.get_item_prototype("iron-axe"), 1});
		mytask->auto_craft_from( {&factorio.get_item_prototype("iron-plate")}, &factorio );
		mytask->actions_changed();
		splayers[player_idx].scheduler.add_task(mytask); // FIXME DEBUG
	}


	start_mines_t start_mines = find_start_mines(&factorio, &gui);

	struct facility_t
	{
		string name;
		vector<PlannedEntity> entities;
		int level = 0; // this is the desired level
		int actual_level = 0;

		facility_t(string n, const vector<PlannedEntity>& e) : name(n), entities(e), level(0) {}
	};
	
	std::array<facility_t,4> facilities = {
		facility_t("coal", plan_early_coal_rig(*start_mines.coal, &factorio)),
		facility_t("iron", plan_early_smelter_rig(*start_mines.iron, &factorio)),
		facility_t("copper", plan_early_smelter_rig(*start_mines.copper, &factorio)),
		facility_t("stone", plan_early_chest_rig(*start_mines.stone, &factorio)) };

	while (true)
	{
		bool consistent_state = factorio.parse_packet( factorio.read_packet() );
		frame++;
		//cout << "frame " << frame << endl; if (frame>1000) break; // useful for profiling with gprof / -pg option, since we must cleanly exit then (not by ^C)
		
		GUI::wait(0.001);

		if (!consistent_state)
			continue;

		for (auto& player : factorio.players)
		{
			auto& splayer = splayers[player.id];
			
			splayer.tick_scheduler(&factorio, player);
		}

		if (int key = gui.key())
		{
			Logger log("menu");
			switch(key)
			{
				case '1':
				case '2':
				case '3':
				case '4':
				{
					facility_t& facility = facilities[key - '1'];
					facility.level++;
					int new_level = facility.level;

					auto task = make_shared<Task>(facility.name + " facility upgrade ("+to_string(facility.level)+")");
					task->finished_callback = [&facility, new_level](){ facility.actual_level = new_level; };
					task->priority_ = 0;
					task->goals.emplace();
					for (const auto& ent : facility.entities) if (ent.level < facility.level)
						task->goals->push_back(make_unique<goal::PlaceEntity>(ent));
					
					if ((key=='2' || key=='1') && facility.level == 1) // special handling for the first iron drill or coal
					{
						if (key=='2')
						{
							// fill with some wood
							const EntityPrototype* miner = &factorio.get_entity_prototype("burner-mining-drill");
							const EntityPrototype* furnace = &factorio.get_entity_prototype("stone-furnace");
							const int n_wood = 4;
							for (const auto& ent : facility.entities) if (ent.level < facility.level)
							{
								if (ent.proto == miner)
									task->goals->push_back(make_unique<goal::InventoryPredicate>(ent, Inventory{{&factorio.get_item_prototype("raw-wood"), n_wood*2}}, INV_FUEL));
								else if (ent.proto == furnace)
									task->goals->push_back(make_unique<goal::InventoryPredicate>(ent, Inventory{{&factorio.get_item_prototype("raw-wood"), n_wood}}, INV_FUEL));
							}
						}
						else if (key=='1')
						{
							// add one wood
							task->goals->push_back(make_unique<goal::InventoryPredicate>(facility.entities.front(), Inventory{{&factorio.get_item_prototype("raw-wood"), 1}}, INV_FUEL));
						}
					}
					if (key=='1' && facility.level > 1)
					{
						// put one coal in the coal drill
						task->goals->push_back(make_shared<goal::InventoryPredicate>(
							static_cast<goal::PlaceEntity*>(task->goals->back().get())->entity,
							Inventory{{&factorio.get_item_prototype("coal"), 1}}, INV_FUEL
						));
					}
					
					task->actions_changed();
					task->update_actions_from_goals(&factorio, player_idx); // HACK
					if (key=='2' && facility.level == 1) // special handling for the first iron drill
						task->auto_craft_from({&factorio.get_item_prototype("burner-mining-drill"), &factorio.get_item_prototype("stone-furnace"), &factorio.get_item_prototype("raw-wood")}, &factorio);
					else
						task->auto_craft_from({&factorio.get_item_prototype("iron-plate"), &factorio.get_item_prototype("stone"), &factorio.get_item_prototype("raw-wood"), &factorio.get_item_prototype("coal")}, &factorio);

					splayers[player_idx].scheduler.add_task(task);
					
					break;
				}
				case '5':
				{
					Logger log2("detail");
					const ItemPrototype* coal = &factorio.get_item_prototype("coal");
					int n_coal = 0;
					for (const auto& ent : factorio.actual_entities.within_range(Area(-200,-200,200,200)))
						if (const ContainerData* data = ent.data_or_null<ContainerData>())
						{
							log2 << "considering " << ent.str() << " with fuel_is_output = " << data->fuel_is_output << endl;
							for (const auto& [key,val] : data->inventories)
								if (key.item == coal && (inventory_flags[key.inv].take || (key.inv == INV_FUEL && data->fuel_is_output)))
									n_coal += val;
						}
					n_coal += factorio.players[player_idx].inventory[coal].unclaimed();

					int n_consumers = /*coal: facilities[0].actual_level*4 + */ facilities[1].actual_level*3 + facilities[2].actual_level*3 + facilities[3].actual_level*2;
					int coal_per_furnace = (n_consumers>0) ? n_coal / n_consumers : 0;
					log << "found " << n_coal << " coal ready for use (" << factorio.players[player_idx].inventory[coal].unclaimed() << " in our inventory). we have " << n_consumers << " furnace-equivalent consumers, which get " << coal_per_furnace << " coal each." << endl;


					const EntityPrototype* miner = &factorio.get_entity_prototype("burner-mining-drill");
					const EntityPrototype* furnace = &factorio.get_entity_prototype("stone-furnace");
					auto task = make_shared<Task>("coal refill");
					task->priority_ = -10;
					task->goals.emplace();
					for (size_t i=1; i<4; i++) // facility[0] is coal which does not need to be refilled
					{
						const facility_t& facility = facilities[i];
						for (const PlannedEntity& ent : facility.entities) if (ent.level < facility.actual_level)
						{
							if (ent.proto == miner)
								task->goals->push_back(make_unique<goal::InventoryPredicate>(ent, Inventory{{coal, coal_per_furnace*2}}, INV_FUEL));
							else if (ent.proto == furnace)
								task->goals->push_back(make_unique<goal::InventoryPredicate>(ent, Inventory{{coal, coal_per_furnace}}, INV_FUEL));
						}
					}

					task->actions_changed();
					task->update_actions_from_goals(&factorio, player_idx); // HACK
					splayers[player_idx].scheduler.add_task(task);

					break;
				}
					
				case 'a':
					log << "scheduler.update_item_allocation()" << endl;
					splayers[player_idx].scheduler.update_item_allocation();
					break;

				case 'd':
					factorio.players[player_idx].inventory.dump();
					break;

				case 'r':
					log << "scheduler.recalculate()" << endl;
					splayers[player_idx].scheduler.recalculate();
					break;
				case 's':
				{
					splayers[player_idx].scheduler.dump();
					if (auto task = splayers[player_idx].scheduler.get_current_task())
					{
						gui.clear();
						debug_draw_actions(task->actions.get(), &gui, factorio.players[player_idx].position);
					}
					break;
				}
			}
		}
	}
}
