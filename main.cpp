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

#include <iostream>
#include <climits>
#include <cassert>
#include "factorio_io.h"
#include "scheduler.hpp"
#include "gui/gui.h"

using namespace std;
using namespace sched;

static float cluster_quality(int diam, size_t coal_size, size_t iron_size, size_t copper_size, size_t stone_size)
{
	const float COAL_SIZE = 25*10;
	const float IRON_SIZE = 25*10;
	const float COPPER_SIZE = 25*10;
	const float STONE_SIZE = 25*5;
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
					cout << endl;
					cout << "coal:\t" << coal.second->bounding_box.center().str() << endl;
					cout << "iron:\t" << iron.second->bounding_box.center().str() << endl;
					cout << "copper:\t" << copper.second->bounding_box.center().str() << endl;
					cout << "stone:\t" << stone.second->bounding_box.center().str() << endl;

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

					cout << "diam = " << diam << endl;

					// find the closest water source and calculate the new bounding box
					Pos_f box_center = Pos_f((x1+x2)/2., (y1+y2)/2.);
					Pos best_water_pos;
					int best_water_diam = INT_MAX;
					for (auto water_pos : watersources.around(box_center))
					{
						if ((water_pos.pos - box_center).len() >= 1.5*(best_water_diam-diam/2) )
							break;

						cout << ".";
						
						int new_diam = max(
							max(x2,water_pos.pos.x)-min(x1,water_pos.pos.x),
							max(y2,water_pos.pos.y)-min(y1,water_pos.pos.y) );

						if (new_diam < best_water_diam)
						{
							best_water_diam = new_diam;
							best_water_pos = water_pos.pos;
							cout << "\b*";
						}
					}
					cout << endl;
					cout << "found water at " << best_water_pos.str() << endl;
					cout << "best_water_diam = " << best_water_diam << endl;

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
static void debug_draw_actions(const action::ActionBase* goal, GUI::MapGui* gui, debug_draw_actions_state_t& state)
{
	using namespace action;
	if (auto g = dynamic_cast<const WalkTo*>(goal))
	{
		cout << "WalkTo " << g->destination.str() << endl;
		GUI::Color color = GUI::Color(255,0,255);
		color.blend( GUI::Color(127,127,127), (state.count%5)/4.0);

		gui->line(state.last, g->destination, color);
		gui->rect(g->destination, 2, GUI::Color(255,127,0));

		state.last = g->destination;
		state.count++;
	}
	else if (auto g = dynamic_cast<const TakeFromInventory*>(goal))
	{
		cout << "TakeFromInventory" << endl;
		gui->rect(state.last,1, GUI::Color(0,127,255));
	}
	else if (auto g = dynamic_cast<const CompoundGoal*>(goal))
	{
		cout << "recursing into CompoundGoal" << endl;
		for (const auto& sub : g->subgoals)
			debug_draw_actions(sub.get(), gui, state);
	}
	else if (auto g = dynamic_cast<const ParallelGoal*>(goal))
	{
		cout << "recursing into ParallelGoal" << endl;
		for (const auto& sub : g->subgoals)
			debug_draw_actions(sub.get(), gui, state);
	}
}
static void debug_draw_actions(const action::ActionBase* goal, GUI::MapGui* gui, Pos start)
{
	debug_draw_actions_state_t state;
	state.last = start;
	debug_draw_actions(goal, gui, state);
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
		if (packet == "STATIC_DATA_END")
			break;
		else
			factorio.parse_packet(packet);
	}
	cout << "done reading static data" << endl;

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
		exit(1);
	}
	cout << endl << "Player index = " << player_idx << endl << endl;

	Scheduler scheduler(&factorio, player_idx);

	auto mytask = make_shared<Task>("mytask");
	mytask->required_items.push_back({&factorio.get_item_prototype("assembling-machine-1"), 5});
	mytask->auto_craft_from({&factorio.get_item_prototype("iron-plate"), &factorio.get_item_prototype("copper-plate")}, &factorio);
	mytask->priority_ = 5;
	mytask->dump();

	cout << "\n\n" << endl;

	scheduler.add_task(mytask);

	while (true)
	{
		factorio.parse_packet( factorio.read_packet() );
		frame++;
		//cout << "frame " << frame << endl; if (frame>1000) break; // useful for profiling with gprof / -pg option, since we must cleanly exit then (not by ^C)
		
		for (auto& player : factorio.players)
		{
			if (player.goals)
			{
				if (player.goals->is_finished())
					player.goals = nullptr;
				else
					player.goals->tick();
			}
		}

		if (int key = gui.key())
		{
			switch(key)
			{
				case 'r':
					scheduler.recalculate();
					cout << "scheduler.recalculate()" << endl;
					break;
				case 's':
				{
					scheduler.dump();
					auto task = scheduler.get_current_task();
					gui.clear();
					debug_draw_actions(&task->actions, &gui, factorio.players[scheduler.player_idx].position);
					break;
				}
			}
		}

		#if 0 // demonstration of primitive actions
		if (online && frame == 1000)
		{
			start_mines_t start_mines = find_start_mines(&factorio, &gui);

			for (auto& player : factorio.players) if (player.connected)
			{
				cout << "WALKING"<< endl;
				player.goals = make_unique<action::CompoundGoal>(&factorio, player.id);
				
				{
				auto parallel = make_unique<action::ParallelGoal>(&factorio, player.id);
				
				// craft an axe
				parallel->subgoals.emplace_back( make_unique<action::CraftRecipe>(
					&factorio, player.id, "iron-axe", 1) );
				
				auto seq = make_unique<action::CompoundGoal>(&factorio, player.id);

				seq->subgoals.emplace_back( make_unique<action::HaveItem>(
					&factorio, player.id, "raw-wood", 60) );

				// make 2 chests, place a drill and a furnace on the iron field and manually make 15 stone
				auto parallel2 = make_unique<action::ParallelGoal>(&factorio, player.id);
				parallel2->subgoals.emplace_back( make_unique<action::CraftRecipe>(
					&factorio, player.id, "wooden-chest", 2) );

				auto seq2 = make_unique<action::CompoundGoal>(&factorio, player.id);

				Pos iron_position = start_mines.iron->positions[0]; // FIXME
				seq2->subgoals.emplace_back( make_unique<action::WalkAndPlaceEntity>(
					&factorio, player.id, "burner-mining-drill", iron_position, d8_NORTH) );
				seq2->subgoals.emplace_back( make_unique<action::WalkAndPlaceEntity>(
					&factorio, player.id, "stone-furnace", iron_position-Pos(0,2)) );

				seq2->subgoals.emplace_back( make_unique<action::PutToInventory>(
					&factorio, player.id, "raw-wood", 20,
					Entity(iron_position, &factorio.get_entity_prototype("burner-mining-drill")),
					INV_FUEL) );
				seq2->subgoals.emplace_back( make_unique<action::PutToInventory>(
					&factorio, player.id, "raw-wood", 10,
					Entity(iron_position-Pos(0,2), &factorio.get_entity_prototype("stone-furnace")),
					INV_FUEL) );
				
				seq2->subgoals.emplace_back( make_unique<action::WalkAndMineResource>(
					&factorio, player.id, start_mines.stone, 15) );

				parallel2->subgoals.emplace_back( move(seq2) );

				seq->subgoals.push_back( move(parallel2) );

				parallel->subgoals.push_back(move(seq));

				player.goals->subgoals.emplace_back(move(parallel));
				}
				
				player.goals->start();
			}
		}
		#endif

		GUI::wait(0.001);
	}
}
