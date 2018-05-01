#include "scheduler.hpp"
#include "inventory.hpp"
#include "player.h"
#include "worldlist.hpp" // FIXME
#include "pathfinding.hpp"
#include "defines.h"
#include "factorio_io.h"
#include "safe_cast.hpp"

#include <boost/functional/hash.hpp>

#include <vector>
#include <utility>
#include <memory>
#include <limits>
#include <optional>

using namespace std;

namespace sched
{

void Scheduler::tick()
{
	tick_crafting_queue();
	
}

void Scheduler::tick_crafting_queue()
{
	
}

static map<const ItemPrototype*, signed int> needed_items(const vector<ItemStack>& required_items, const CraftingList& crafting_list)
{
	map<const ItemPrototype*, signed int> needed;

	for (const ItemStack& stack : required_items)
		needed[stack.proto] += stack.amount;

	// FIXME: this assumes that recipes are sane, and don't have cyclic dependencies.
	// If you can make 1 Bar from 2 Foo, and you can make 1 Foo and 1 Qux from a Bar,
	// then you do need *two* Foos in the beginning. This naive algorithm will output
	// a need of one single Foo, however.
	for (const auto& entry : crafting_list.recipes)
	{
		switch (entry.status)
		{
			case CraftingList::PENDING:
				for (const auto& ingredient : entry.recipe->ingredients)
					needed[ingredient.item] += ingredient.amount;
				[[fallthrough]];
			case CraftingList::CURRENT:
				for (const auto& product : entry.recipe->products)
					needed[product.item] -= product.amount;
				[[fallthrough]];
			case CraftingList::FINISHED:
				break;
		}
	}
	
	return needed;
}

std::vector<ItemStack> Task::get_missing_items(Inventory inventory) const
{
	auto needed = needed_items(required_items, crafting_list);
	
	vector<ItemStack> missing;
	for (auto [item, needed_amount] : needed)
	{
		if (safe_cast<signed int>(inventory[item]) < needed_amount)
			missing.push_back(ItemStack{item, size_t(needed_amount-inventory[item])});
	}
	return missing;
}

bool Task::check_inventory(Inventory inventory)
{
	auto needed = needed_items(required_items, crafting_list);
	
	for (auto [item, needed_amount] : needed)
	{
		if (safe_cast<signed int>(inventory[item]) < needed_amount)
			return false;
	}
	return true;
}

// returns an ordering in which the tasks should perform their crafts,
// balancing between "high priority first" and "quick crafts may go first"
vector<weak_ptr<Task>> Scheduler::calc_crafting_order()
{
	struct WaitingQueueItem
	{
		shared_ptr<Task> task;
		Clock::duration time_granted; // time we've already granted to someone else
		Clock::duration max_granted;
		Clock::duration own_duration;
	};
	vector<WaitingQueueItem> queue;

	// grocery store queue algorithm: from the highest to lowest priority
	// task, enqueue their crafts with their expected total runtime
	// each task that is enqueued asks their precedessors to let them skip them.
	// precedessors will approve unless their total time_granted exceeds 10% of
	// their own_duration.
	for (auto& iter : pending_tasks)
	{
		auto& task = iter.second;

		queue.push_back({
			task,
			Clock::duration::zero(),
			task->crafting_list.time_remaining()/10, // magic number
			task->crafting_list.time_remaining()
		});
		
		// jump the queue
		for (size_t i = queue.size(); i --> 1;)
		{
			auto& curr = queue[i];
			auto& pred = queue[i-1];
			if (pred.time_granted + curr.own_duration <= pred.max_granted)
			{
				// we may skip that one
				pred.time_granted += curr.own_duration;
				std::swap(curr,pred);
				continue;
			}
			else
			{
				// nope
				break;
			}
		}
	}

	vector<weak_ptr<Task>> result;
	for (auto& iter : queue)
		result.push_back(iter.task);
	return result;
}

// updates the crafting order and the tasks' ETAs
void Scheduler::update_crafting_order()
{
	for (auto task_w : crafting_order)
		if (auto task = task_w.lock()) // silently ignore expired weak_ptrs
			task->crafting_eta = nullopt;

	crafting_order = calc_crafting_order();

	auto eta = Clock::duration::zero();
	for (auto task_w : crafting_order)
	{
		auto task = shared_ptr<Task>(task_w); // loudly crash on expired weak_ptrs
		eta += task->crafting_list.time_remaining();

		if (task->check_inventory(Inventory(game->players[player_idx].inventory, task)))
			task->crafting_eta = { eta, Clock::now()};
		else
			task->crafting_eta = nullopt;
	}
}

/** Returns a list of up to `max_n` crafts (consisting of the owning task and the recipe) that should be performed next.
 */
vector<pair<weak_ptr<Task>,const Recipe*>> Scheduler::get_next_crafts(size_t max_n)
{
	vector<pair<weak_ptr<Task>,const Recipe*>> result;

	for (auto& task_w : crafting_order)
	{
		shared_ptr<Task> task(task_w); // fail loudly if the weak_ptr has expired
		auto& crafts = task->crafting_list;
		Inventory available_inventory(game->players[player_idx].inventory, task);
		if (crafts.almost_finished())
			continue;

		// iterate over each not-yet-finished craft in the current task's crafting list
		// and add all crafts  which we will be able to perform it with our current inventory
		// (taking into account the inventory changes made by previous crafts)
		for (auto& entry : crafts.recipes)
		{
			switch (entry.status)
			{
				case CraftingList::FINISHED:
					break;
				case CraftingList::PENDING:
					if (available_inventory.apply(entry.recipe))
					{
						result.emplace_back(task, entry.recipe);

						// limit the result's size
						if (result.size() >= max_n)
							goto finish;
					}
					break;
				case CraftingList::CURRENT:
					available_inventory.apply(entry.recipe, true);
					break;
			}
		}
	}
finish:
	
	return result;
}



using schedule_key_t = pair<Clock::duration, Task::priority_t>;
using schedule_t = multimap< schedule_key_t, shared_ptr<Task> >;

static int sec(Clock::duration d)
{
	return chrono::duration_cast<chrono::seconds>(d).count();
}

static void dump_schedule(const schedule_t& schedule, int n_columns = 80, int n_ticks = 5)
{
	if (schedule.empty())
		return;
	
	Clock::duration last_offset = schedule.rbegin()->first.first + schedule.rbegin()->second->duration;

	for (const auto& [key, task] : schedule)
	{
		const auto& [begin_offset, priority] = key;
		Clock::duration end_offset = begin_offset+task->duration;
		string label = to_string(sec(begin_offset)) + " " + task->name + " " + to_string(sec(end_offset));

		int begin_column = n_columns * begin_offset.count() / last_offset.count();
		int end_column = n_columns * end_offset.count() / last_offset.count();
		end_column = max(end_column, begin_column+1);

		string result;
		if (end_column - begin_column - 4 > int(label.length()))
		{
			int pad = end_column - begin_column - 4 - label.length();
			int padl = pad/2;
			int padr = pad-padl;
			result = string(begin_column, ' ') + "<" + string(padl, '=') + " " + label + " " + string(padr, '=') + ">";
		}
		else if (end_column - begin_column - 2 > int(label.length()))
		{
			int pad = end_column - begin_column - 2 - label.length();
			int padl = pad/2;
			int padr = pad-padl;
			result = string(begin_column, ' ') + "<" + string(padl, '=') + label + string(padr, '=') + ">";
		}
		else if (end_column + 1 + int(label.length()) < n_columns)
			result = string(begin_column, ' ') + "<" + string(end_column-begin_column-2, '=') + "> "+ label;
		else if (begin_column - 1 >= int(label.length()))
			result = string(begin_column - 1 - label.length(), ' ') + label + " <" + string(end_column-begin_column-2, '=') + ">";
		else
			result = string(begin_column, ' ') + "<" + string(end_column-begin_column-2, '=') + ">\n^ " + label + " ^";

		cout << result << endl;
	}


	int sec = chrono::duration_cast<chrono::seconds>(last_offset).count();
	int tick_seconds = pow(10,floor(log10(sec/n_ticks)));
	auto tick_duration = chrono::duration_cast<Clock::duration>(chrono::seconds(tick_seconds));
	int lastcol = 0;
	int tickval;
	int tickid = 0;
	for (auto i = Clock::duration::zero(); i < last_offset; i+=tick_duration, tickid++)
	{
		char tickchr;
		if (tickid % 10 == 0)
			tickchr = '|';
		else if (tickid % 5 == 0)
			tickchr = ':';
		else
			tickchr = '.';

		tickval = chrono::duration_cast<chrono::seconds>(i).count();
		int col = n_columns * i.count() / last_offset.count();
		cout << string(col-lastcol, ' ') << tickchr;
		lastcol = col+1;
	}
	cout << " " << tickval << " sec" << endl;
}

/** Checks a schedule using operator(), while caching and reusing pathfinding results. */
struct ScheduleChecker
{
	using calc_walk_duration_t = std::function<Clock::duration(Pos,Pos,float)>;

private:
	Pos start_location;
	calc_walk_duration_t calc_walk_duration;
	
	
	struct cache_hash {
		size_t operator()(tuple<Pos,Pos,float> const& t) const {
			size_t h = 0;
			boost::hash_combine(h, hash<Pos>{}(get<0>(t)));
			boost::hash_combine(h, hash<Pos>{}(get<1>(t)));
			boost::hash_combine(h, hash<float>{}(get<2>(t)));
			return h;
		}
	};

	unordered_map<tuple<Pos,Pos,float>, Clock::duration, cache_hash> cache;
public:
	/** @param calc_walk_duration_ User-supplied function that calculates a walking duration from "from, to, radius" */
	ScheduleChecker(Pos start_location_, calc_walk_duration_t calc_walk_duration_) : start_location(start_location_), calc_walk_duration(calc_walk_duration_) {}

	/** Returns whether a given schedule only bears acceptable delays for all scheduled tasks.
	 *  First, the ways between the subsequent tasks are calculated. With that knowledge, the
	 *  real schedule is created by assigning each task the earliest possible starting point,
	 *  but no earlier than their originally scheduled starting point. If that earliest possible
	 *  starting point exceeds the originally scheduled starting point by too much, the schedule
	 *  is considered unacceptable and false is returned.
	 */
	bool operator()(const schedule_t& schedule)
	{
		// TODO: support multiple start/endlocations, greedily select the closest start location
		Clock::duration prev_finished_offset = Clock::duration::zero();
		const Clock::duration acceptable_delay = std::chrono::seconds(10);
		Pos last_location = start_location;
		for (const auto& entry : schedule)
		{
			/** time offset when the task wants to start actually doing its work */
			const Clock::duration& work_offset = entry.first.first;
			const shared_ptr<Task>& task = entry.second;
			
			/** time offset when the previous task needs to have finished in
			 *  order to start walking to the current task in order to be on time */
			Clock::duration walk_duration = get_walk_duration(last_location, task->start_location, task->start_radius);
			
			Clock::duration actual_work_offset = max(
				work_offset,
				prev_finished_offset + walk_duration
			);

			if (actual_work_offset - work_offset > acceptable_delay)
				return false;
			
			prev_finished_offset = actual_work_offset + task->duration;
		}
		return true;
	}

private:
	Clock::duration get_walk_duration(Pos from, Pos to, float radius)
	{
		auto key = make_tuple(from,to,radius);
		auto iter = cache.find(key);
		if (iter == cache.end())
		{
			Clock::duration walk_duration = calc_walk_duration(from, to, radius);
			iter = cache.insert(make_pair(key, walk_duration)).first;
		}
		return iter->second;
	}

};


/** returns a pointer (or nullptr) to a task which may or may not be contained in the pending_tasks queue.
  * If it is contained, then this task has or will have its requirements fulfilled at the time
  * it reaches the task location.
  * If the returned task is not contained, this is a item-collecting task. It will never
  * reside in the pending_tasks list, but only in the current_task field. It may be safely
  * removed from there (which should call its destructor), and will be regenerated if neccessary.
  * TODO: what to do with crafts?
  */
shared_ptr<Task> Scheduler::get_next_task()
{
	const Player& player = game->players[player_idx];
	// FIXME: use proper calculation function
	ScheduleChecker check_schedule(player.position, [](Pos a, Pos b, float radius) { return std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(200) * (a-b).len()); });
	schedule_t schedule;
	
	/*// TODO move this somewhere else!
	for (auto& entry : pending_tasks)
	{
		// Maybe recalculate, our crafting_list. I.e. what items from
		// required_items should be fetched from a chest, and which
		// should be crafted. This method will most likely just return
		// and re-use the cached path. (Except for the first call, where
		// no cached value is available yet)
		task.recalc_crafting_path();


		// If the player has useful "free for all"-items in the inventory,
		// grab them. This is fine, because all higher-priority tasks
		// have already executed this loop and grabbed their items, so
		// we're not stealing them.
		task.claim_available_items();
	}*/

	for (const auto& [prio, pending_task]  : pending_tasks)
	{
		shared_ptr<Task> task;

		if (pending_task->eventually_runnable())
			task = pending_task;
		else
		{
			// we need a resource-collecting task right now. (It will be immediately
			// runnable)
			assert(schedule.empty() || schedule.begin()->first.second < prio);
			
			/* If a task is already scheduled, then it has a higher
			   priority than the currently considered task. In that
			   case, the resource-collecting task's duration is
			   limited by that already-scheduled task plus some
			   grace time. OTOH, all tasks we may consider
			   afterwards would have a priority smaller than our
			   task.  So we try to collect our resources and as
			   many resources for other tasks as possible within
			   the available time. (Only limited by some sanity
			   constant) */
			const auto grace_duration = std::chrono::duration_cast<Clock::duration>(chrono::seconds(15)); // FIXME magic number, better retrieve this from the task. probably related to task's ETA
			Clock::duration max_duration;
			if (schedule.empty())
				max_duration = Clock::duration::max();
			else
				max_duration = schedule.begin()->first.first + grace_duration;
			
			task = build_collector_task(pending_task, max_duration);

			if (task == nullptr)
			{
				cout << "note: tried to build a collector task for task '" << pending_task->name << "'\n"
				        "      with a time limit of " << 
				        chrono::duration_cast<chrono::seconds>(max_duration).count() << " sec "
				        "but failed" << endl;
				continue;
			}
			
			assert(task->crafting_list.time_remaining() == Clock::duration::zero());
		}
		assert(task != nullptr);

		Clock::duration eta = task->crafting_list.time_remaining();
		auto iter = schedule.insert(make_pair( make_pair(eta, task->priority()), task));

		cout << "desired schedule:" << endl;
		dump_schedule(schedule);
		cout << "actual schedule:" << endl;
		dump_schedule(check_schedule.sanitize(schedule));

		if (!check_schedule(schedule))
		{
			// the Task would delay a more important task for too long
			// => cannot use it
			cout << "-> not okay, reverting" << endl;
			schedule.erase(iter);
		}
		else
		{
			cout << "-> okay :)" << endl;
			// the Task can be scheduled in `eta`.
			if (eta <= chrono::seconds(10)) // FIXME magic value
			{
				return task;
			}
		}

		assert(check_schedule(schedule));
	}
	return nullptr;
}




constexpr double WALKING_SPEED = 8.9021; // in tiles per second.
// source: https://wiki.factorio.com/Talk:Exoskeleton#Movement_speed_experiments

static double path_length(const vector<Pos>& path) // FIXME move this somewhere else
{
	double sum = 0.;
	for (size_t i=1; i<path.size(); i++)
		sum += (path[i-1]-path[i]).len();
	return sum;
}

static Clock::duration path_walk_duration(const vector<Pos>& path) // FIXME move this somewhere else
{
	return chrono::duration_cast<Clock::duration>(
		chrono::duration<float>(
			path_length(path) / WALKING_SPEED
		)
	);
}

static Clock::duration walk_duration_approx(const Pos& start, const Pos& end)
{
	return chrono::duration_cast<Clock::duration>(
		chrono::duration<float>(
			(start-end).len() / WALKING_SPEED
		)
	);
}

static double walk_distance_in_time(Clock::duration time) // FIXME move this somewhere else
{
	using fseconds = chrono::duration<float>;
	auto seconds = chrono::duration_cast<fseconds>(time).count();
	return seconds * WALKING_SPEED;
}

template <typename Rep, typename Per>
std::ostream& operator<<(std::ostream& os, const chrono::duration<Rep,Per>& dur)
{
	return os << chrono::duration_cast<chrono::seconds>(dur).count();
}

/** returns a Task containing walking and take_from actions in order to make
 * one or more Tasks eventually_runnable. This is a trivial implementation
 * returning a very suboptimal path which only respects original_task, no
 * opportunistic detours are made. But it gets the stuff done.*/
shared_ptr<Task> Scheduler::build_collector_task(const shared_ptr<Task>& original_task, Clock::duration max_duration, float grace)
{
	UNUSED(grace); // this is a poor man's implementation
	const WorldList<ItemStorage>& item_storages = game->item_storages;

	const Player& player = game->players[player_idx];

	auto missing_items = original_task->get_missing_items(Inventory(player.inventory, original_task));
	// TODO: desired_items, which are basically missing_items + 200. we consider chests only if
	// missing_items > 0, but then we take as many items from it so that desired_items is satisfied.

	// assert that missing_items only has unique entries
	#ifndef NDEBUG
	{
		set<const ItemPrototype*> s;
		for (ItemStack& stack : missing_items)
		{
			assert(s.count(stack.proto) == 0);
			s.insert(stack.proto);
		}
	}
	#endif

	// initialize the resulting collector task
	auto result = make_shared<Task>(game, player_idx, "resource collector for " +original_task->name);
	result->start_location = player.position;
	result->start_radius = std::numeric_limits<decltype(result->start_radius)>::infinity();
	result->is_dependent = true;
	result->owner = original_task;

	Clock::duration time_spent = Clock::duration::zero();

	// for all chests close to the player, if they can contribute to
	// missing_items, append a CompoundGoal that walks there and gets
	// the items
	Pos last_pos = player.position;
	const int ALLOWED_DISTANCE = 2;
	for (const auto& chest : item_storages.around(player.position))
	{
		// exit condition: if the chest is outside of a
		// (remaining_walktime + walktime(player.pos -> last_pos))-radius
		// from the center "player.pos", then it's guaranteed to be outside of
		// a (remaining_walktime)-radius from last_pos as well. in that case,
		// we're done.
		if (walk_duration_approx(last_pos, chest.entity.pos) + time_spent - walk_duration_approx(player.position, last_pos) > max_duration)
		{
			cout << "aborting due to duration approximation" << endl;
			break;
		}

		bool relevant = false;
		auto chest_goal = make_unique<action::CompoundGoal>(game, player.id);

		// check whether the chest is useful for any missing item.
		// if so, set relevant = true and construct the chest goal
		// if not, chest_goal will be deleted at the end of this scope.
		chest_goal->subgoals.push_back(make_unique<action::WalkTo>(game, player.id, chest.entity.pos, ALLOWED_DISTANCE));

		auto new_missing_items = missing_items;
		cout << "missing: ";
		bool missing_anything = false;
		for (ItemStack& stack : new_missing_items)
		{
			if (stack.amount == 0)
				continue;

			cout << stack.proto->name << "(" << stack.amount << "), ";
			missing_anything = true;

			auto iter = chest.inventory.find(stack.proto);
			if (iter == chest.inventory.end())
				continue;

			size_t take_amount = min(iter->second, stack.amount);
			stack.amount -= take_amount;
			chest_goal->subgoals.push_back(make_unique<action::TakeFromInventory>(
				game, player.id, stack.proto->name,
				take_amount, chest.entity, INV_CHEST));
			relevant = true;
		}
		cout << endl;

		if (!missing_anything)
		{
			cout << "we've got everything we need" << endl;
			break;
		}

		if (relevant)
		{
			cout << "calculating path from " << last_pos.str() << " to " << chest.entity.pos.str() << " with a length limit of " << walk_distance_in_time(max_duration - time_spent) << endl;
			// check if this chest is actually close enough
			auto path = a_star(
				last_pos, chest.entity.pos,
				game->walk_map,
				ALLOWED_DISTANCE, 0.,
				walk_distance_in_time(max_duration - time_spent)
			);
			
			cout << "\t->" << path.size() << endl;

			Clock::duration chest_duration = path_walk_duration(path); // FIXME maybe add a constant?
			
			if (time_spent + chest_duration <= max_duration)
			{
				cout << "visiting chest at " << chest.entity.pos.str() << " for ";
				for (const auto& sg : chest_goal->subgoals)
					if (auto action = dynamic_cast<action::TakeFromInventory*>(sg.get()))
						cout << action->item << "(" << action->amount << "), ";
				cout << "\b\b with a cost of " <<
					chrono::duration_cast<chrono::seconds>(chest_duration).count() << " sec (" <<
					chrono::duration_cast<chrono::seconds>(max_duration-time_spent).count() << 
					" sec remaining)" << endl;

				result->actions.subgoals.push_back(move(chest_goal));
				result->end_location = chest.entity.pos;
				last_pos = chest.entity.pos;
				time_spent += chest_duration;

				missing_items = move(new_missing_items);
			}
			// else: we can't afford going there. maybe try somewhere else.
			else
			{
				cout << "not visiting chest at " << chest.entity.pos.str() << " for ";
				for (const auto& sg : chest_goal->subgoals)
					if (auto action = dynamic_cast<action::TakeFromInventory*>(sg.get()))
						cout << action->item << "(" << action->amount << "), ";
				cout << "\b\b with a cost of " <<
					chrono::duration_cast<chrono::seconds>(chest_duration).count() << " sec (" <<
					chrono::duration_cast<chrono::seconds>(max_duration-time_spent).count() << 
					" sec remaining)" << endl;
			}
		}
		// else: chest_goal goes out of scope and is deleted
		else
		{
			cout << "not visiting chest at " << chest.entity.pos.str() << " (irrelevant)" << endl;
		}
	}

	if (result->actions.subgoals.empty())
		return nullptr;
	
	result->duration = time_spent;

	return result;
}

#if 0
WORK IN PROGRESS. this shall build a collector task by solving the travelling
purchaser problem for the first task, and then iteratively adding detours for
collecting items needed by other non-eventually-runnable pending tasks, until
a maximum detour cost is reached.

Phase 0: cluster the chests on the map into "markets":
	for each unvisited chest:
		visit all chests within a certain range of this chest
		and put them into a common market
		(note that "certain range" implies that no water or other
		borders are in between)
Phase 1: solve TPP for all items needed by original_task
	genetic algo?

Phase 2: add other tasks until "max_duration" or "(1+grace/100) * phase1_solution.duration"
         is exceeded
	for all considered tasks, list items they would also need
	for all items, calculate their cost for collecting them (or parts of)
	for all such detours, incrementally add the detour with the best cost/use ratio


/** returns a Task containing walking and take_from actions in order to make
 * one or more Tasks eventually_runnable.  At least `original_task` will be
 * considered. It will be handled completely unless handling it is impossible
 * due to a lack of items, or would exceed max_duration. In this case, it's
 * handled partially.  More tasks from pending_tasks are considered, if they
 * are different from `original_task`, if collecting their items will extend the
 * collection duration by no more than `grace` percent and will not take longer
 * than max_duration.
 */
shared_ptr<Task> Scheduler::build_collector_task(const shared_ptr<Task>& original_task, const Player& player, Clock::duration max_duration, float grace)
{
	// conventions:
	// solution vectors contain a sequence of indices in `markets`.
	// edge_id i always refers to the edge between node i and (i+1).

	struct Market
	{
		struct SubItemStorage
		{
			const ItemPrototype* proto;
			size_t amount;
			Pos location;
		};

		Pos location;
		vector<SubItemStorage> items;
	};

	vector<Market> markets = ...; // TODO: cluster the chests into Markets

	// TODO: solve the TPP problem for the original_task.
	vector<size_t> original_solution = approximate_travelling_purchaser(/*FIXME*/);


	struct Detour
	{
		size_t edge_id; // '0' means "between path.node[0] and path.node[1]"
		Clock::duration cost;

		// path should have `updated_edge` and `updated_edge+1` as new edges
		void update(const vector<Market>& markets, const vector<size_t>& path, size_t updated_edge)
		{
			bool needs_full_recalc = false;

			if (updated_edge == SIZE_MAX)
				needs_full_recalc = true;
			else if (updated_edge == edge_id)
			{
				auto cost1 = calc_cost(markets, path, updated_edge);
			}
			else
			{
				// nothing to do, except updating indices that may have changed
				if (edge_id > updated_edge)
					edge_id++;
			}
		}
	};

	/** add collects for subsequent tasks until we exceed either max_duration or grace */
	for (auto& entry : pending_tasks) if (!task_eventually_runnable(entry.second, player.inventory))
	{
		const shared_ptr<Task>& task = entry.second;
		vector<ItemStack> items = task->get_missing_items();
		vector<Detour> item_detours(items.size());
		for (auto& detour : item_detours)
			detour.update(path);

	}
}
#endif

} // namespace sched