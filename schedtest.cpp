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
	EntityPrototype chest = EntityPrototype("chest", "container", "", {}, true);
} entities;

struct {
	Recipe circuit =
		{"circuit", true, 10.0,
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
		{"furnace", true, 14.5,
			vector<Recipe::ItemAmount>{
				{&items.stone, 10},
				{&items.iron, 4}
			},
			vector<Recipe::ItemAmount>{
				{&items.furnace, 1}
			}
		};
} recipes;

static void test_get_next_craft(FactorioGame* game, int playerid)
{
	sched::Scheduler sched(game, playerid);
	Player& p = game->players[playerid];
	p.inventory.clear();
	
	std::array<shared_ptr<sched::Task>, 3> tasks;

	tasks[0] = make_shared<sched::Task>(game, playerid, "task #0");
	tasks[0]->priority_ = 42;
	for (int i=0; i<1; i++)
		tasks[0]->crafting_list.recipes.push_back({sched::CraftingList::PENDING, &recipes.circuit});
	for (int i=0; i<1; i++)
		tasks[0]->crafting_list.recipes.push_back({sched::CraftingList::PENDING, &recipes.machine});
	
	tasks[1] = make_shared<sched::Task>(game, playerid, "task #1");
	tasks[1]->priority_ = 17;
	for (int i=0; i<20; i++)
		tasks[1]->crafting_list.recipes.push_back({sched::CraftingList::PENDING, &recipes.furnace});
	
	tasks[2] = make_shared<sched::Task>(game, playerid, "task #2");
	tasks[2]->priority_ = 99;
	for (int i=0; i<5; i++)
		tasks[2]->crafting_list.recipes.push_back({sched::CraftingList::PENDING, &recipes.circuit});

	p.inventory[&items.iron].amount += 135;
	p.inventory[&items.iron].claims.push_back({tasks[0],40});
	p.inventory[&items.iron].claims.push_back({tasks[1],80});
	p.inventory[&items.iron].claims.push_back({tasks[2],15});
	p.inventory[&items.coppercable].amount += 83;
	p.inventory[&items.coppercable].claims.push_back({tasks[0],40});
	p.inventory[&items.coppercable].claims.push_back({tasks[2],5});
	p.inventory[&items.circuit].amount = 3;
	p.inventory[&items.circuit].claims.push_back({tasks[0],3});
	p.inventory[&items.stone].amount += 252;
	p.inventory[&items.stone].claims.push_back({tasks[1],230});


	for (auto& task : tasks)
	{
		cout << task->name << " -> crafting_list.time_remaining() = " << chrono::duration_cast<chrono::milliseconds>(task->crafting_list.time_remaining()).count() << "ms" << endl;
	}
	
	for (auto& task : tasks)
		sched.pending_tasks.insert({task->priority(), task});


	auto allocation = sched.allocate_items_to_tasks();
	sched.update_crafting_order(allocation);

	cout << "dumping crafting_order:" << endl;
	for (auto task_w : sched.crafting_order)
	{
		if (auto task = task_w.lock())
		{
			cout << "\ttask " << task->name << " with priority " << task->priority() << " has ";
			if (task->eventually_runnable())
				cout << "eta = " << chrono::duration_cast<chrono::seconds>(task->crafting_eta->eta).count() << "s of which " << chrono::duration_cast<chrono::seconds>(sched::Clock::now() - task->crafting_eta->reference).count() << "s have already passed" << endl;
			else
				cout << "no eta yet" << endl;
		}
	}

	auto result = sched.get_next_crafts(allocation, 20);
	cout << "got " << result.size() << " crafts" << endl;

	for (const auto& [task, recipe] : result)
	{
		cout << "\tcraft " << recipe->name << " for task " << task.lock()->name << endl;
	}
}

static void test_get_missing_items(const shared_ptr<sched::Task>& task, const Inventory& inv)
{
	cout << "task '" << task->name << "' is missing the following items:" << endl;
	for (const ItemStack& is : task->get_missing_items(inv))
		cout << "\t" << is.proto->name << ": " << is.amount << endl;
	cout << "\t(end)" << endl << endl;
}

static void test_get_next_task_internal(sched::Scheduler& sched, vector<shared_ptr<sched::Task>> tasks)
{
	sched.pending_tasks.clear();
	cout << string(80,'-') << "\ntesting with the following tasks: ";
	for (auto t : tasks)
	{
		sched.pending_tasks.insert({t->priority(), t});
		cout << t->name << "(" << t ->priority() << ") ";
	}
	cout << endl;
	
	auto allocation = sched.allocate_items_to_tasks();
	sched.update_crafting_order(allocation);
	
	auto next_task = sched.get_next_task(allocation);
	cout << "next task is " << ((next_task == nullptr) ? "<null>" : next_task->name) << "\n\n"
	     << string(80, '-') << endl << endl;
}

static void test_get_next_task(FactorioGame* game, int playerid)
{
	sched::Scheduler sched(game, playerid);
	Player& p = game->players[playerid];
	p.inventory.clear();

	auto greedy_task = make_shared<sched::Task>(game, playerid, "greedy task");
	greedy_task->priority_ = 42;
	greedy_task->start_location = greedy_task->end_location = Pos(10,10);
	greedy_task->start_radius = 1;
	greedy_task->duration = chrono::minutes(2);
	greedy_task->required_items = { {&items.iron, 17}, {&items.belt, 42} };
	
	auto crafting_task = make_shared<sched::Task>(game, playerid, "crafting task");
	crafting_task->priority_ = 44;
	crafting_task->start_location = crafting_task->end_location = Pos(10,10);
	crafting_task->start_radius = 1;
	crafting_task->duration = chrono::minutes(2);
	crafting_task->required_items = { {&items.circuit, 4}, {&items.iron, 42} };
	crafting_task->crafting_list.recipes = {
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.circuit},
		{sched::CraftingList::PENDING, &recipes.circuit},
		{sched::CraftingList::PENDING, &recipes.circuit},
		{sched::CraftingList::PENDING, &recipes.circuit}
	};

	auto overcrafting_task = make_shared<sched::Task>(game, playerid, "overcrafting task");
	overcrafting_task->priority_ = 44;
	overcrafting_task->start_location = overcrafting_task->end_location = Pos(10,10);
	overcrafting_task->start_radius = 1;
	overcrafting_task->duration = chrono::minutes(2);
	overcrafting_task->required_items = { {&items.circuit, 4}, {&items.iron, 42} };
	overcrafting_task->crafting_list.recipes = {
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.circuit},
		{sched::CraftingList::PENDING, &recipes.circuit},
		{sched::CraftingList::PENDING, &recipes.circuit},
		{sched::CraftingList::PENDING, &recipes.circuit}
	};

	auto undercrafting_task = make_shared<sched::Task>(game, playerid, "undercrafting task");
	undercrafting_task->priority_ = 44;
	undercrafting_task->start_location = undercrafting_task->end_location = Pos(10,10);
	undercrafting_task->start_radius = 1;
	undercrafting_task->duration = chrono::minutes(2);
	undercrafting_task->required_items = { {&items.circuit, 4}, {&items.iron, 42} };
	undercrafting_task->crafting_list.recipes = {
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.coppercable},
		{sched::CraftingList::PENDING, &recipes.circuit},
		{sched::CraftingList::PENDING, &recipes.circuit},
		{sched::CraftingList::PENDING, &recipes.circuit},
		{sched::CraftingList::PENDING, &recipes.circuit}
	};


	auto modest_task = make_shared<sched::Task>(game, playerid, "modest task");
	modest_task->priority_ = 100;
	modest_task->start_location = modest_task->end_location = Pos(10,10);
	modest_task->start_radius = 1;
	modest_task->duration = chrono::minutes(2);
	
	auto quick_modest_task = make_shared<sched::Task>(game, playerid, "quick_modest task");
	quick_modest_task->priority_ = 100;
	quick_modest_task->start_location = quick_modest_task->end_location = Pos(10,10);
	quick_modest_task->start_radius = 1;
	quick_modest_task->duration = chrono::seconds(47);
	

	// this task will have a very long way to walk, compared with a very early ETA
	// and a short actual period of activity.
	auto far_important_task = make_shared<sched::Task>(game, playerid, "far important task");
	far_important_task->priority_ = 1;
	far_important_task->start_location = far_important_task->end_location = Pos(100,100);
	far_important_task->start_radius = 1;
	far_important_task->duration = chrono::seconds(4);
	far_important_task->crafting_list.recipes = { {sched::CraftingList::PENDING, &recipes.coppercable} };
	
	auto far_nice_task = make_shared<sched::Task>(game, playerid, "far nice task");
	far_nice_task->priority_ = 100;
	far_nice_task->start_location = far_nice_task->end_location = Pos(-100,-100);
	far_nice_task->start_radius = 1;
	far_nice_task->duration = chrono::seconds(4);
	far_nice_task->crafting_list.recipes = { {sched::CraftingList::PENDING, &recipes.coppercable} };


	test_get_missing_items(greedy_task, Inventory());
	test_get_missing_items(crafting_task, Inventory());
	test_get_missing_items(overcrafting_task, Inventory());
	test_get_missing_items(undercrafting_task, Inventory());
	test_get_missing_items(modest_task, Inventory());

	test_get_next_task_internal(sched, {modest_task});
	test_get_next_task_internal(sched, {crafting_task});
	test_get_next_task_internal(sched, {greedy_task});

	auto& inv_copper = game->players[playerid].inventory[&items.copper];
	inv_copper.amount = 208;
	inv_copper.claims.push_back(TaggedAmount::Tag{ crafting_task, 200 });
	inv_copper.claims.push_back(TaggedAmount::Tag{ far_important_task, 4 });
	inv_copper.claims.push_back(TaggedAmount::Tag{ far_nice_task, 4 });
	auto& inv_iron = game->players[playerid].inventory[&items.iron];
	inv_iron.amount = 200;
	inv_iron.claims.push_back(TaggedAmount::Tag{ crafting_task, 200 });

	cout << "now with some inventory" << endl;
	test_get_next_task_internal(sched, {modest_task, crafting_task});
	test_get_next_task_internal(sched, {quick_modest_task, crafting_task});
	
	test_get_next_task_internal(sched, {far_important_task});
	test_get_next_task_internal(sched, {crafting_task, far_nice_task});


}

int main()
{


	FactorioGame game("dummy");


	pathfinding::walk_t walkable;
	walkable.known=true;
	walkable.can_walk=true;
	walkable.can_cross=true;
	for (double& w : walkable.margins) w=1.;

	for (int x=-1000; x<1000; x++)
		for (int y=-1000; y<1000; y++)
			game.walk_map.at(x,y) = walkable;

	const int playerid = 0;
	game.players.resize(playerid+1);
	game.players[playerid].id = playerid;
	game.players[playerid].connected = true;

	game.item_storages.insert( ItemStorage{
		Entity{ {10,-2}, &entities.chest},
		{	{&items.belt, 821},
		 	{&items.copper, 17} } });
	game.item_storages.insert( ItemStorage{
		Entity{ {100,30}, &entities.chest},
		{	{&items.iron, 513} } });
	game.item_storages.insert( ItemStorage{
		Entity{ {10,70}, &entities.chest},
		{	{&items.copper, 513} } });
	game.item_storages.insert( ItemStorage{
		Entity{ {1000,7000}, &entities.chest},
		{	{&items.stone, 0} } });

	//sched::Scheduler sched(&game, playerid);


	test_get_next_craft(&game, playerid);
	cout << "\n\n" << string(80,'=') << "\n\n";
	test_get_next_task(&game, playerid);
	exit(0);
	
	/*auto task1 = make_shared<sched::Task>(&game, playerid);
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
*/
}
