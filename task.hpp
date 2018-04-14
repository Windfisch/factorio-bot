#include <vector>
#include <chrono>

#include "item.h"
#include "action.hpp"
	

/*
// inactive tasks waiting for execution
prio_queue< std::unique_ptr<Task> > pending_tasks;

// tasks that are semi-scheduled, i.e. their dependencies are being resolved
// (i.e., scheduled in our crafting activity thread)
fifo< std::unique_ptr<Task> > preparing_tasks;

// the one task that is actually executing actions. (i.e., controlling our
// position/walking/acting activity thread)
// this can either be for actually executign its actions, or for collecting
// items or walking to the start location. in case of collecting, the task
// might be put to preparing_tasks again.
CurrentTask active_task;
*/


// put to active_task will: calculate collects, crafts and way-to-walk
// 1. if collects are open, execute them
// 2. if crafts are pending, move the task over to preparing_tasks. track the ETA. end.
// 3. otherwise, walk to `location`
// 4. start executing `actions`


// whenever a craft is done or active_task becomes empty:
// if active_task is empty:
//   tmpETA = infinity
//   iterate through all pending tasks, ordered by priority
//     if endETA < tmpETA:
//       tmpETA = beginETA + grace
//       if runnable: move to active_task. end.
//     endif

// positive grace: allow for some delays for more imporant tasks
// negative grace: enforce safety margin at the cost of efficiency

/*
struct CurrentTask
{
	unique_ptr<Task> task;

}*/
