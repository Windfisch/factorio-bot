#include <chrono>
#include <memory>
#include <vector>
#include <assert.h>
#include <map>
#include <optional>

#include "task.hpp"
#include "inventory.hpp"
#include "player.h"
#include "action.hpp"
#include "recipe.h"
#include "worldlist.hpp" // FIXME remove

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
};

struct Task
{
	Task(FactorioGame* game_, int player_, std::string name_) : name(name_), actions(game_,player_) {}

	std::string name;

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
	
	// returns true if the given inventory is sufficient to execute all crafts from
	// crafting_list, and then supply all items listed in required_items
	bool check_inventory(Inventory inventory) const;
	
	// returns a list of items needed to a) fully execute the crafting_list and
	// b) have all required_items afterwards.
	std::map<const ItemPrototype*, signed int> missing_items() const;

	// returns a list of items missing from the given inventory, that are required
	// to a) fully execute the crafting_list and b) have all required_items afterwards.
	std::vector<ItemStack> get_missing_items(Inventory inventory) const;

	action::CompoundGoal actions;

	// optimizes the task, if possible (e.g. TSP-solving for get-item-tasks)
	// virtual void optimize() = 0; FIXME

	// calculates a Task with no required_items, that collects required_items for this
	// task and assigns them into this Task's ownership. This task will have
	// `is_dependent` set to true and `owner` set to this task.
	// also calculates CraftingQueueItem, which is a list of crafts that have to be
	// made.
	std::pair<Task, CraftingList> prepare() const;

};

// Scheduler for all Tasks for a single player. Note that this does *not*
// do cross-player load balancing. It it necessary to remove Tasks from one
// Player and to insert them at another Player's scheduler to accomplish this.
struct Scheduler
{
	Scheduler(FactorioGame* game_, int player_) : game(game_), player_idx(player_) {}

	FactorioGame* game;
	int player_idx;

	Clock clock;


	struct Appointment
	{
		Clock::time_point when;
		Pos where;
		float radius;
	};

	// list of all tasks known to the scheduler
	std::multimap<int, std::shared_ptr<Task>> pending_tasks;


	using item_allocation_t = std::map<const Task*, Inventory>;

	// allocation of the player's inventory to the tasks
	item_allocation_t allocate_items_to_tasks() const;
	
	// list of the first N tasks (or all tasks? TODO) in their order
	// the grocery store queue sort has determined
	std::vector<std::weak_ptr<Task>> crafting_order;

	// returns an ordering in which the tasks should perform their crafts,
	// balancing between "high priority first" and "quick crafts may go first"
	std::vector<std::weak_ptr<Task>> calc_crafting_order();
	
	// updates the crafting order and the tasks' ETAs
	void update_crafting_order(const item_allocation_t& task_inventories);

	std::vector<std::pair<std::weak_ptr<Task>,const Recipe*>> get_next_crafts(const item_allocation_t& task_inventories, size_t max_n = 20);
	std::shared_ptr<Task> get_next_task(const item_allocation_t& task_inventories);

	void invariant() const
	{
	#if 0 // FIXME reenable this
	//#ifndef NDEBUG
		for (const auto& [prio, task] : pending_tasks)
		{
			if (task->is_dependent)
				assert(!task->owner.expired());

			assert(prio == task->priority());

			//if (auto craft = task->crafting_queue_item.lock())
			//	assert(craft->owner.lock() == task);
		}

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


	void tick();

private:
	void tick_crafting_queue();
};

}
