#include <chrono>
#include <memory>
#include <vector>
#include <assert.h>
#include <map>
#include <optional>

#include "task.hpp"
#include "inventory.hpp"
#include "action.hpp"
#include "recipe.h"

class FactorioGame;


namespace sched
{

using Clock = std::chrono::steady_clock;

struct CraftingList
{
	enum status_t {
		PENDING, // not yet started. the ingredients haven't been consumed, the results aren't there yet.
		CURRENT, // currently in the crafting queue. the ingredients have already been consumed, the results aren't there yet
		FINISHED // finished. the ingredients have been consumed, and the results have been added
	};
	struct Entry {
		status_t status;
		const Recipe* recipe;
	};
	std::vector<Entry> recipes;

	// returns the time needed for all recipes which are not FINISHED. this means
	// that an almost-finishe, but yet CURRENT recipe is accounted for with its
	// full duration.
	Clock::duration time_remaining() const
	{
		Clock::duration sum = Clock::duration::zero();
		for (const Entry& ent : recipes)
			if (ent.status != FINISHED)
				sum += ent.recipe->crafting_duration();
		return sum;
	}

	// returns true if all crafts are FINISHED
	bool finished() const
	{
		for (auto [status, recipe] : recipes)
			if (status != FINISHED)
				return false;
		return true;
	}
	// returns true if all crafts are FINISHED or CURRENT
	bool almost_finished() const
	{
		for (auto [status, recipe] : recipes)
			if (status == PENDING)
				return false;
		return true;
	}

	void invariant() const
	{
	#ifndef NDEBUG
		int n_current = 0;
		for (auto& recipe : recipes)
			if (recipe.status == CURRENT)
				n_current++;
		assert(n_current == 0 || n_current == 1);
	#endif
	}
};

struct Task
{
	using priority_t = int;

	std::string name;
	priority_t priority_;
	
	action::CompoundGoal actions;
	
	CraftingList crafting_list; // could be computed from required_items, available item storages and the crafting pressure
	

	// to be able to start this task, we must be in a start_radius circle around start_location
	// TODO FIXME: actually, we can replace this by "the position of the first
	// goal that is not yet completed"
	Pos start_location; // first noncompleted goal's location
	float start_radius; // probably a constant

	// the duration for executing the actual task, excluding all resource acquirance
	Clock::duration duration; // sum of goal walktimes and goal execution times
	
	// after `duration` has passed, we'll end up here, free for other tasks
	Pos end_location; // last goal's location

	std::vector<ItemStack> required_items; // can be calculated from goals

	Task(FactorioGame* game_, int player_, std::string name_) : name(name_), actions(game_,player_) {}

// private:
	
	// if the task is dependent, then this task is a item-collection-task
	// for "owner" (though it may also collect for some other tasks, because
	// that's on its way).
	// this task will become obsolete immediately if `owner` gets deleted
	// for some reason.
	// if de-scheduling this task (i.e. replacing the current task by something
	// else, then it's only written back to pending_tasks if is_dependent is false.
	bool is_dependent = false;
	std::weak_ptr<Task> owner;

	struct crafting_eta_t {
		Clock::duration eta; // managed by Scheduler
		Clock::time_point reference;
	};
	std::optional<crafting_eta_t> crafting_eta;

	bool eventually_runnable() const { return crafting_eta.has_value(); }
	

	void invariant() const
	{
		//assert(!is_dependent || crafting_queue_item == nullptr);
	}

	static constexpr int LOWEST_PRIO = 99999;
	static constexpr int HIGHEST_PRIO = -99999;

	priority_t priority() const
	{
		if (!is_dependent)
			return priority_;
		else if (auto o = owner.lock())
			return o->priority();
		else
			return LOWEST_PRIO;
	}

	// returns true if the given inventory is sufficient to execute all crafts from
	// crafting_list, and then supply all items listed in required_items
	bool check_inventory(Inventory inventory) const;
	
	// returns a list of items needed to a) fully execute the crafting_list and
	// b) have all required_items afterwards.
	std::map<const ItemPrototype*, signed int> missing_items() const;

	// returns a list of items missing from the given inventory, that are required
	// to a) fully execute the crafting_list and b) have all required_items afterwards.
	std::vector<ItemStack> get_missing_items(Inventory inventory) const;

	// optimizes the task, if possible (e.g. TSP-solving for get-item-tasks)
	// virtual void optimize() = 0; FIXME

};

// Scheduler for all Tasks for a single player. Note that this does *not*
// do cross-player load balancing. It it necessary to remove Tasks from one
// Player and to insert them at another Player's scheduler to accomplish this.
struct Scheduler
{
	using owned_recipe_t = std::pair< std::weak_ptr<Task>, const Recipe* >;

	Scheduler(FactorioGame* game_, int player_) : game(game_), player_idx(player_) {}
	
	/** adds a task to pending_tasks 
	  *
	  * No task may be inserted twice.
	  *
	  * While a task is in pending_tasks, its priority
	  * may not be changed. Remove and add the task for this purpose.
	  *
	  * Note that this does *not* imply recalculate()
	  */
	void add_task(std::shared_ptr<Task> task);

	/** removes a task.
	  *
	  * Note that this does *not* imply recalculate()a
	  */
	void remove_task(std::shared_ptr<Task> task);
	
	/** returns a pointer (or nullptr) to a task which may or may not be contained
	  * in the pending_tasks queue.
	  *
	  * If it is contained, then this task has or will have its requirements
	  * fulfilled at the time it reaches the task location.
	  *
	  * If the returned task is not contained, this is a item-collecting task. It
	  * will never reside in the pending_tasks list, but only in the current_task
	  * field. It may be safely removed from there (which should call its
	  * destructor), and will be regenerated if neccessary.
	  *
	  * @see schedule_t
	  */
	std::shared_ptr<Task> get_current_task();

	/** returns the current craft as (owning task, Recipe) pair,
	  * or nullopt if no craft can be done.
	  *
	  * The craft should usually be possible with the available inventory.
	  * @see current_crafting_list.
	  */
	std::optional< owned_recipe_t > peek_current_craft();

	/** changes the current craft's state from PENDING to CURRENT. 
	  *
	  * It is expected that the ingredients have already been removed from the
	  * inventory, and that current_item_allocation has been updated.
	  */
	void accept_current_craft();

	/** changes the current craft's state from CURRENT to PENDING.
	  *
	  * There may be no {accept,confirm,retreat}_current_craft() calls between
	  * this call and the accept_current_craft() call that has accepted the given craft.
	  *
	  * It is expected that the ingredients have already been returned to the
	  * inventory, and that current_item_allocation has been updated.
	  */
	void retreat_current_craft(owned_recipe_t craft);

	/** confirms the finalisation of a craft by changing its state from
	  * CURRENT to FINISHED and removing it from the crafting list.
	  *
	  * There may be no {accept,confirm,retreat}_current_craft() calls between
	  * this call and the accept_current_craft() call that has accept-ed the given craft.
	  *
	  * It is expected that the products have already been inserted into the
	  * inventory, and that current_item_allocation has been updated.
	  */
	void confirm_current_craft(owned_recipe_t craft);

	/** full recalculation of all schedules.
	  *
	  * First, the item allocation to the tasks is recomputed.
	  *
	  * Second, the "grocery store sort" is performed and the
	  * tasks' executable crafts are arranged in the crafting order.
	  * Simultaneously, the tasks' crafting_eta are recalculated.
	  *
	  * Third, the task schedule is recalculated, and if required, an item
	  * collector task is inserted at the beginning of the schedule.
	  */
	void recalculate();


// private:

	FactorioGame* game;
	int player_idx;

	Clock clock;


	struct Appointment
	{
		Clock::time_point when;
		Pos where;
		float radius;
	};

	/** list of all tasks known to the scheduler.
	  *
	  * invariant: for each entry, key == value->priority() */
	std::multimap<int, std::shared_ptr<Task>> pending_tasks;

	std::multimap<int, std::shared_ptr<Task>>::iterator find_task(std::shared_ptr<Task> task);
	
	using schedule_key_t = std::pair<Clock::duration, Task::priority_t>;
	/** describes a schedule: this map is ordered by what was the ETA of the
	  * task was when the schedule was created. (And by the task priority, as
	  * a second element.)
	  */
	using schedule_t = std::multimap< schedule_key_t, std::shared_ptr<Task> >;
	using item_allocation_t = std::map<const Task*, Inventory>;
	using crafting_list_t = std::vector<owned_recipe_t>;

	item_allocation_t current_item_allocation;
	schedule_t current_schedule;

	/** List of the next few upcoming crafts, that were possible to do
	  * with the inventory which we had when recalculate() was last called */
	crafting_list_t current_crafting_list;
	Clock::time_point calculation_timestamp;

	/** @see get_current_task */
	[[deprecated]] std::shared_ptr<Task> get_next_task(const item_allocation_t& task_inventories);

	/** allocation of the player's inventory to the tasks */
	item_allocation_t allocate_items_to_tasks() const;
	
	// list of the first N tasks (or all tasks? TODO) in their order
	// the grocery store queue sort has determined
	std::vector<std::weak_ptr<Task>> crafting_order;

	/** returns an ordering in which the tasks should perform their crafts,
	  * balancing between "high priority first" and "quick crafts may go first" */
	std::vector<std::weak_ptr<Task>> calc_crafting_order();
	
	/** updates the crafting order and the tasks' ETAs */
	void update_crafting_order(const item_allocation_t& task_inventories);

	[[deprecated("use calculate_crafts instead")]] crafting_list_t get_next_crafts(const item_allocation_t& task_inventories, size_t max_n = 20) { return calculate_crafts(task_inventories, max_n); }
	crafting_list_t calculate_crafts(const item_allocation_t& task_inventories, size_t max_n = 20);
	

	/** returns a (possibly empty) schedule of tasks which may or may not be
	  * contained in the pending_tasks queue.
	  *
	  * If a task is contained, then it has or will have its requirements
	  * fulfilled at the time the bot reaches the task location. (Taking into
	  * account the walking time)
	  *
	  * If the returned task is not contained, this is a item-collecting task. It
	  * will never reside in the pending_tasks list, but only in the current_task
	  * field. It may be safely removed from there (which should call its
	  * destructor), and will be regenerated if neccessary.
	  *
	  * Calculation is stopped once a task is found that is runnable within
	  * the time specified by eta_threshold.
	  *
	  * @see schedule_t
	  */
	schedule_t calculate_schedule(const item_allocation_t& task_inventories, Clock::duration eta_threshold);

	void invariant() const
	{
	#ifndef NDEBUG
		for (const auto& [prio, task] : pending_tasks)
		{
			if (task->is_dependent)
				assert(!task->owner.expired());

			assert(prio == task->priority());

			//if (auto craft = task->crafting_queue_item.lock())
			//	assert(craft->owner.lock() == task);
		}
		#if 0 // TODO cleanup
		int n_currently_crafting = 0;
		for (const auto& [prio, craft] : pending_crafts)
		{
			assert(!craft->owner.expired());

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
	std::shared_ptr<Task> build_collector_task(const item_allocation_t& task_inventories, const std::shared_ptr<Task>& original_task, Clock::duration max_duration, float grace = 10.f);
};

}
