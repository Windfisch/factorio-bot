#include <chrono>
#include <memory>
#include <vector>
#include <assert.h>
#include <map>

#include "task.hpp"
#include "inventory.hpp"
#include "player.h"
#include "action.hpp"

struct Recipe;

namespace sched
{

using Clock = std::chrono::steady_clock;

struct CraftingList
{
	std::vector< std::pair<bool, const Recipe*> > recipes;

	// `current_recipe` ranges between 0 and recipes.size(), or is SIZE_MAX.
	// if ==SIZE_MAX, we're currently not crafting and craft_{start,end}_time
	// are invalid. otherwise, recipes[ current_recipe ] denotes the recipe
	// which is currently being crafted and craft_start_time and
	// craft_end_time denote the begin and end time, respectively.
	size_t current_recipe;
	Clock::time_point craft_start_time;
	Clock::time_point craft_end_time;
	bool currently_crafting;

	Clock::duration time_lost_if_interrupted(Clock::time_point now) const
	{
		assert(currently_crafting);
		return now-craft_start_time;
	}

	Clock::duration time_remaining(Clock::time_point now) const
	{
		assert(currently_crafting);
		Clock::duration result = craft_end_time - now;
		return min(result, Clock::duration::zero());
	}

	// may only be called if the finished craft has been confirmed.
	// returns the Recipe to be crafted now, or none.
	std::optional<Recipe> advance(Clock::time_point now)
	{
		if (currently_crafting)
			current_recipe++;

		if (current_recipe >= recipes.size())
		{
			currently_crafting = false;
			return std::nullopt;
		}

		currently_crafting = true;
		craft_start_time = now;
		craft_end_time = now + recipes[current_recipe].duration;
		return std::optional(recipes[current_recipe]);
	}

	bool finished() const { return current_recipe >= recipes.size(); }
};

struct Task
{
	// if the task is dependent, then this task is a item-collection-task
	// for "owner" (though it may also collect for some other tasks, because
	// that's on its way).
	// this task will become obsolete immediately if `owner` gets deleted
	// for some reason.
	// if de-scheduling this task (i.e. replacing the current task by something
	// else, then it's only written back to pending_tasks if is_dependent is false.
	bool is_dependent;
	std::weak_ptr<Task> owner;

	CraftingList crafting_list;

	void invariant() const
	{
		assert(!is_dependent || crafting_queue_item == nullptr);
	}

	static constexpr int LOWEST_PRIO = 99999;

	using priority_t = int;
	
	priority_t priority_;
	priority_t priority() const
	{
		if (!is_dependent)
			return priority_;
		else if (auto o = owner.lock())
			return o->priority();
		else
			return LOWEST_PRIO;
	}

	// to be able to start this task, we must be in a start_radius circle around start_location
	Pos start_location;
	float start_radius;

	// the duration for executing the actual task, excluding all resource acquirance
	Clock::duration duration;
	
	// after `duration` has passed, we'll end up here, free for other tasks
	Pos end_location;

	std::vector<ItemStack> required_items;
	
	std::vector<ItemStack> get_missing_items(); // TODO

	action::CompoundGoal actions;

	// optimizes the task, if possible (e.g. TSP-solving for get-item-tasks)
	virtual void optimize() = 0;

	// calculates a Task with no required_items, that collects required_items for this
	// task and assigns them into this Task's ownership. This task will have
	// `is_dependent` set to true and `owner` set to this task.
	// also calculates CraftingQueueItem, which is a list of crafts that have to be
	// made.
	std::pair<Task, CraftingList> prepare() const;

};
	
class Scheduler
{
	Scheduler(FactorioGame* game_) : game(game_) {}

	FactorioGame* game;

	Clock clock;


	struct Appointment
	{
		Clock::time_point when;
		Pos where;
		float radius;
	};

	// list of all tasks known to the scheduler
	std::multimap<int, std::shared_ptr<Task>> pending_tasks;


	std::vector<std::pair<std::weak_ptr<Task>,const Recipe*>> get_next_crafts(Player& player, Clock::time_point now, size_t max_n = 20);
	std::shared_ptr<Task> get_next_task(Player& player, Clock::time_point now);

	/** returns whether we have claimed, or will eventually have crafted, everything the task needs */
	bool task_eventually_runnable(const std::shared_ptr<Task>& task, const TaggedInventory& inventory) const
	{
		for (const auto& req : task->required_items)
		{
			auto remaining = req.amount;

			const auto& claims = inventory.at(req.proto).claims;
			auto idx = claims.find(std::weak_ptr(task));
			if (idx != SIZE_MAX)
				remaining -= std::min(req.amount, claims[idx].amount);

			if (auto cqi = task->crafting_queue_item.lock())
			{
				// FIXME figure out how much net outcome your crafting queue has.
			}

			if (remaining > 0)
				return false;
		}
		return true;
	}

	void invariant() const
	{
	#ifndef NDEBUG
		for (const auto& [prio, task] : pending_tasks)
		{
			if (task->is_dependent)
				assert(!task->owner.is_expired());

			assert(prio == task->priority());

			if (auto craft = task->crafting_queue_item.lock())
				assert(craft->owner.lock() == task;
		}

		int n_currently_crafting = 0;
		for (const auto& [prio, craft] : pending_crafts)
		{
			assert(!craft->owner.is_expired());

			assert(prio == craft->owner->priority());
			
			assert(craft->current_recipe <= craft->recipes.size());
			assert(craft->current_recipe < craft->recipes.size() || !craft->currently_crafting);
			
			if (craft->currently_crafting)
				n_currently_crafting++;

			auto o = craft->owner.lock();
			assert(o);
			assert(o->crafting_queue_item.lock() == craft);
		}
		assert(n_currently_crafting <= 1);
	#endif
	}

	

	/** returns a Task containing walking and take_from actions in order to make
	 * one or more Tasks eventually_runnable.  At least `original_task` will be
	 * considered. It will be handled completely unless handling it is impossible
	 * due to a lack of items, or would exceed max_duration. In this case, it's
	 * handled partially.  More tasks from pending_tasks are considered, if they
	 * are different from `original_task`, if collecting their items will extend the
	 * collection duration by no more than `grace` percent and will not take longer
	 * than max_duration.
	 */
	std::shared_ptr<Task> build_collector_task(const std::shared_ptr<Task>& original_task, const Player& player, Clock::duration max_duration, float grace = 10.f);


	void tick();

private:
	void tick_crafting_queue();
};

}
