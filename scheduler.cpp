#include "scheduler.hpp"
#include "inventory.hpp"
#include "player.h"
#include "worldlist.hpp" // FIXME
#include "pathfinding.hpp"
#include "defines.h"
#include "factorio_io.h"

#include <boost/functional/hash.hpp>

#include <vector>
#include <utility>
#include <memory>
#include <limits>

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


/** Returns a list of up to `max_n` crafts (consisting of the owning task and the recipe) that should be performed next.
 *  Crafts belonging to higher priority tasks will generally be performed earlier, with two exceptions:
 *  1. Low-priority quick crafts are preferred over heavy-weight high priority crafts (up to a certain limit)
 *  2. If no more higher priority crafts can be performed, we fill the list up with less priority crafts
 *  It is guaranteed that no higher-priority craft gets delayed more than a configurable duration by any lower-priority crafts.
 */
vector<pair<weak_ptr<Task>,const Recipe*>> Scheduler::get_next_crafts(Player& player, Clock::time_point now, size_t max_n)
{
	vector<pair<weak_ptr<Task>,const Recipe*>> result;

	struct WaitingQueueItem
	{
		shared_ptr<Task> task;
		Clock::duration time_granted; // time we've already granted to someone else
		Clock::duration max_granted;
		Clock::duration own_duration;
	};
	vector<WaitingQueueItem> queue;

	// grocery store queue algorithm: from the highest to lowest priority
	// task, enqueue their crafts with their expected total runtime, but only
	// if they can do anything *right now*.
	// each task that is enqueued asks their precedessors to let them skip them.
	// precedessors will approve unless their total time_granted exceeds 10% of
	// their own_duration.
	for (auto& iter : pending_tasks)
	{
		auto& task = iter.second;
		queue.push_back({
			task,
			Clock::duration::zero(),
			task->crafting_list.time_remaining(now)/10,
			task->crafting_list.time_remaining(now)
		});
		
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


	for (auto& iter : pending_tasks)
	{
		auto& task = iter.second;
		auto& crafts = task->crafting_list;
		Inventory available_inventory(player.inventory, task);
		if (crafts.finished())
			continue;

		// iterate over each not-yet-finished craft in the current task's crafting list
		// and add all crafts  which we will be able to perform it with our current inventory
		// (taking into account the inventory changes made by previous crafts)
		for (auto& entry : crafts.recipes) if (!entry.first)
			if (available_inventory.apply(entry.second))
			{
				result.emplace_back(task, entry.second);

				// limit the result's size
				if (result.size() >= max_n)
					goto finish;
			}
	}
finish:
	
	return result;
}



using schedule_key_t = pair<Clock::duration, Task::priority_t>;
using schedule_t = multimap< schedule_key_t, shared_ptr<Task> >;

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
		Clock::duration prev_finished_offset = Clock::duration(0);
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
shared_ptr<Task> Scheduler::get_next_task(Player& player, Clock::time_point now)
{
	// FIXME: use proper calculation function
	ScheduleChecker check_schedule(player.position, [](Pos a, Pos b, float radius) { return std::chrono::duration_cast<Clock::duration>(std::chrono::milliseconds(200) * (a-b).len()); });
	schedule_t schedule;
	for (auto& entry : pending_tasks)
	{
		Task::priority_t prio = entry.first; UNUSED(prio);
		const shared_ptr<Task>& pending_task = entry.second;
		shared_ptr<Task> task;

		if (task_eventually_runnable(pending_task, player.inventory))
			task = pending_task;
		else
		{
			// we need a resource-collecting task right now. (It will be immediately
			// runnable)
			assert(schedule.empty() || schedule.begin()->first.second < prio);
			
			// If a task is already scheduled, then it has a higher
			// priority than the currently considered task. In that
			// case, the resource-collecting task's duration is
			// limited by that already-scheduled task plus some
			// grace time. OTOH, all tasks we may consider
			// afterwards would have a priority smaller than our
			// task.  So we try to collect our resources and as
			// many resources for other tasks as possible within
			// the available time. (Only limited by some sanity
			// constant)
			const auto grace_duration = std::chrono::duration_cast<Clock::duration>(chrono::seconds(15)); // FIXME magic number, better retrieve this from the task. probably related to task's ETA
			Clock::duration max_duration;
			if (schedule.empty())
				max_duration = Clock::duration::max();
			else
				max_duration = schedule.begin()->first.first + grace_duration;
			
			task = build_collector_task(pending_task, player, max_duration);
			
			assert(task->crafting_list.time_remaining(now) == chrono::seconds(0));
		}

		// FIXME: task can be nullptr!
		Clock::duration eta = task->crafting_list.time_remaining(now);
		auto iter = schedule.insert(make_pair( make_pair(eta, task->priority()), task));

		if (!check_schedule(schedule))
		{
			// the Task would delay a more important task for too long
			// => cannot use it
			schedule.erase(iter);
		}
		else
		{
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


struct Chest // FIXME move this to somewhere sane
{
	Pos_f get_pos() const { return entity.pos; } // WorldList<Chest> wants to call this.
	
	Entity entity;
	Inventory inventory;
};



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

/** returns a Task containing walking and take_from actions in order to make
 * one or more Tasks eventually_runnable. This is a trivial implementation
 * returning a very suboptimal path which only respects original_task, no
 * opportunistic detours are made. But it gets the stuff done.*/
shared_ptr<Task> Scheduler::build_collector_task(const shared_ptr<Task>& original_task, const Player& player, Clock::duration max_duration, float grace)
{
	UNUSED(grace); // this is a poor man's implementation

	const WorldList<Chest> world_chests; // FIXME actually get them from the world
	auto missing_items = original_task->get_missing_items();

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
	auto result = make_shared<Task>(game, this->player /*FIXME remove this->*/);
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
	for (const auto& chest : world_chests.around(player.position))
	{
		// exit condition: if the chest is outside of a
		// (remaining_walktime + walktime(player.pos -> last_pos))-radius
		// from the center "player.pos", then it's guaranteed to be outside of
		// a (remaining_walktime)-radius from last_pos as well. in that case,
		// we're done.
		if (walk_duration_approx(last_pos, chest.entity.pos) > max_duration - time_spent + walk_duration_approx(player.position, last_pos))
			break;

		bool relevant = false;
		auto chest_goal = make_unique<action::CompoundGoal>(game, player.id);

		// check whether the chest is useful for any missing item.
		// if so, set relevant = true and construct the chest goal
		// if not, chest_goal will be deleted at the end of this scope.
		chest_goal->subgoals.push_back(make_unique<action::WalkTo>(game, player.id, chest.entity.pos, ALLOWED_DISTANCE));
		for (ItemStack& stack : missing_items)
		{
			if (stack.amount == 0)
				continue;

			auto iter = chest.inventory.find(stack.proto);
			if (iter == chest.inventory.end())
				continue;

			size_t take_amount = min(iter->second, stack.amount);
			stack.amount -= take_amount;
			chest_goal->subgoals.push_back(make_unique<action::TakeFromInventory>(
				game, player.id, stack.proto->name,
				take_amount, chest.entity, INV_CHEST));
			// TODO FIXME attribute this to the respective task
			// (which is currently trivially just original_task)
			relevant = true;
		}

		if (relevant)
		{
			// check if this chest is actually close enough
			Clock::duration chest_duration = path_walk_duration( a_star(
				last_pos, chest.entity.pos,
				game->walk_map,
				ALLOWED_DISTANCE, 0.,
				walk_distance_in_time(max_duration - time_spent)
			) ); // FIXME maybe add a constant?
			
			if (time_spent + chest_duration <= max_duration)
			{
				result->actions.subgoals.push_back(move(chest_goal));
				result->end_location = chest.entity.pos;
				time_spent += chest_duration;
			}
			// else: we can't afford going there. maybe try somewhere else.
		}
		// else: chest_goal goes out of scope and is deleted
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
		struct SubChest
		{
			const ItemPrototype* proto;
			size_t amount;
			Pos location;
		};

		Pos location;
		vector<SubChest> items;
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
