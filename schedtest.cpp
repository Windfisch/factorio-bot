#include "scheduler.hpp"
#include "factorio_io.h"

#include "recipe.h"

#include <memory>
using namespace std;

struct {
	ItemPrototype iron = ItemPrototype("iron", "", nullptr, 100, 0.,0.,0.);
	ItemPrototype copper = ItemPrototype("copper", "", nullptr, 100, 0.,0.,0.);
	ItemPrototype coppercable = ItemPrototype("coppercable", "", nullptr, 100, 0.,0.,0.);
	ItemPrototype stone = ItemPrototype("stone", "", nullptr, 100, 0.,0.,0.);
	ItemPrototype circuit = ItemPrototype("circuit", "", nullptr, 100, 0.,0.,0.);
	ItemPrototype machine = ItemPrototype("machine", "", nullptr, 100, 0.,0.,0.);
	ItemPrototype furnace = ItemPrototype("furnace", "", nullptr, 100, 0.,0.,0.);
	ItemPrototype drill = ItemPrototype("drill", "", nullptr, 100, 0.,0.,0.);
	ItemPrototype belt = ItemPrototype("belt", "", nullptr, 100, 0.,0.,0.);
} items;

struct {
	Recipe circuit =
		{"circuit", true, 1.0,
			vector<Recipe::ItemAmount>{
				{&items.iron, 1},
				{&items.coppercable, 3}
			},
			vector<Recipe::ItemAmount>{
				{&items.circuit, 1}
			}
		};
	Recipe coppercable =
		{"coppercable", true, 0.5,
			vector<Recipe::ItemAmount>{
				{&items.copper, 3}
			},
			vector<Recipe::ItemAmount>{
				{&items.coppercable, 2}
			}
		};
	Recipe machine =
		{"machine", true, 5.0,
			vector<Recipe::ItemAmount>{
				{&items.iron, 10},
				{&items.circuit, 4}
			},
			vector<Recipe::ItemAmount>{
				{&items.machine, 1}
			}
		};
	Recipe furnace =
		{"furnace", true, 5.0,
			vector<Recipe::ItemAmount>{
				{&items.stone, 10},
				{&items.circuit, 4}
			},
			vector<Recipe::ItemAmount>{
				{&items.furnace, 1}
			}
		};
} recipes;

void test_get_next_craft(FactorioGame* game, int playerid)
{
	sched::Scheduler sched(game, playerid);
	
	std::array<shared_ptr<sched::Task>, 3> tasks;

	tasks[0] = make_shared<sched::Task>(game, playerid);
	tasks[0]->priority_ = 42;
	for (int i=0; i<10; i++)
		tasks[0]->crafting_list.recipes.push_back({false, &recipes.circuit});
	for (int i=0; i<2; i++)
		tasks[0]->crafting_list.recipes.push_back({false, &recipes.machine});
	
	tasks[1] = make_shared<sched::Task>(game, playerid);
	tasks[1]->priority_ = 17;
	for (int i=0; i<10; i++)
		tasks[1]->crafting_list.recipes.push_back({false, &recipes.circuit});
	for (int i=0; i<2; i++)
		tasks[1]->crafting_list.recipes.push_back({false, &recipes.furnace});
	
	tasks[2] = make_shared<sched::Task>(game, playerid);
	tasks[2]->priority_ = 99;
	for (int i=0; i<5; i++)
		tasks[2]->crafting_list.recipes.push_back({false, &recipes.circuit});

	for (auto& task : tasks)
	{
		cout << "task->crafting_list.time_remaining() = " << chrono::duration_cast<chrono::milliseconds>(task->crafting_list.time_remaining()).count() << "ms" << endl;
	}
	
	for (auto& task : tasks)
		sched.pending_tasks.insert({task->priority(), task});

	auto result = sched.get_next_crafts(5);
}

int main()
{


	FactorioGame game("dummy");
	const int playerid = 0;
	game.players.resize(playerid+1);
	game.players[playerid].id = playerid;
	game.players[playerid].connected = true;

	sched::Scheduler sched(&game, playerid);


	test_get_next_craft(&game, playerid);
	exit(0);
	
	auto task1 = make_shared<sched::Task>(&game, playerid);
	task1->priority_ = 42;
	task1->start_location = Pos(4,4);
	task1->start_radius = 0;
	task1->end_location = Pos(4,4);
	task1->required_items.push_back({&items.iron, 17});
	task1->required_items.push_back({&items.copper, 17});
	task1->required_items.push_back({&items.machine, 2});

	auto task2 = make_shared<sched::Task>(&game, playerid);
	task2->priority_ = 17;
	task2->start_location = Pos(-1,3);
	task2->start_radius = 0;
	task2->end_location = Pos(-1,3);
	task2->required_items.push_back({&items.belt, 100});
	task2->required_items.push_back({&items.drill, 20});
	task2->required_items.push_back({&items.furnace, 40});

	auto task3 = make_shared<sched::Task>(&game, playerid);
	task3->priority_ = 44;
	task3->start_location = Pos(1,0);
	task3->start_radius = 0;
	task3->end_location = Pos(1,0);
	task3->required_items.push_back({&items.belt, 150});
	task3->required_items.push_back({&items.machine, 20});
	task3->required_items.push_back({&items.circuit, 400});
	task3->required_items.push_back({&items.iron, 200});

	sched.pending_tasks.insert( pair(task1->priority(), task1) );
	sched.pending_tasks.insert( pair(task2->priority(), task2) );
	sched.pending_tasks.insert( pair(task3->priority(), task3) );

//	sched.get_next_task();
//	sched.get_next_crafts();
}
