The Scheduler
=============

The scheduler is, together with the [goal manager](goal_manager.md), the core
of the factorio-bot. It computes the order in which actions from the tasks
are to be executed, manages the crafting queue and collects items as necessary.

The scheduler is instanced *per player*, any inter-player-coordination must
be implemented outside.

Problem statement
-----------------

In factorio, there are two separate threads of activity: The *crafting* thread
and the *walking* thread. Any tasks that have to be performed usually require
both: E.g., building a new iron mine not only requires the bot to *walk* to the
location and actually place the entities on the ground (both of which happen in
the walking thread), but also to *craft* or *collect* the required items first.
Collecting them also occupies the walking thread, while crafting them would be
handled in the *crafting thread*.

**(P1)** For an efficient operation, crafting and walking operations should be
parallelized as much as possible. For example, if a high-priority mine-building
task must craft a lot of mines first, it would be wasted time if all other task
would wait for it to complete. Instead, the bot could perform a lower-priority
task that only requires *walking/building*, efficiently parallelizing with the
crafting process.

**(P2)** However, the lower-priority task should not (significantly) delay the
high priority task, which could happen e.g. if its start location was on the
opposite side of the map.

**(P3)** Additionally, a lower-priority task which could well fit in the idle
gap should be able to execute, even if it needs one tiny craft to succeed. In
this case, the a lower-priority task should be able to perform its crafts
*before* a high priority task, if it doens't delay it for too long.

Concepts
--------

The scheduler makes use of the following core concepts:

- **Tasks**: A task consists of a list of *actions*, together with a *start*
  and *end location* and a list of *required items*. The scheduler must fulfill
  all requirements of the task, i.e. either collect or craft all required items,
  and move the player close enough to the task's start location. Every task has
  a *priority* attached to it.
- **Tagged Inventory**/**claims**: Each item in the player's inventory can be
  *claimed* by a task, meaning that it's reserved for use solely by this task.
  A task may only use items it has claimed. **Unclaimed items** can not be used
  by a task, but can be claimed by any task.
- **Crafting list**: Each task has a crafting list, which is initially empty
  and managed by the scheduler. For each required item of a task, the scheduler
  decides whether to claim it from unclaimed items in the inventory, to collect
  it or to craft it. In the latter case, it's added to the task's crafting list.
- **Crafting order**: The scheduler keeps a list of which task may craft first.
  This is required to fulfill *(P3)*, allowing low-prio-but-short tasks to craft
  first.

A task is either **runnable**, **eventually runnable** or **not runnable**.
It's *runnable* iif it has all required items, and *eventually runnable* iif it
will have all required items after all of its crafts, which will be eventually
performed given the current crafting order. If it's not *eventually runnable*,
a task is considered *not runnable*.

Any *eventually runnable* task can be assigned a **crafting eta**. This is the
time, given the current crafting order and assuming that all crafts in this
crafting order will be performed, that it will take until this task becomes
*runnable*.

A task that is *not runnable* has **required items w.r.t. its crafting list**,
which equates of the minimum item list that, after all of its crafts would be
performed, can supply its *required items*.

Implementation
--------------

### Task selection

We start with an empty **timetable**. A timetable is **valid**, iif all tasks
in it can start no later than a small grace time after their scheduled start,
considering task durations and the time spent from walking from one task to
another. A timetable is **executable**, iif the first task it contains can
start right now (or very soon, at least).

All pending tasks, ordered by decreasing priority, are considered. If the
considered task is **runnable** or **eventually runnable** (after possibly
assigning unclaimed items to it), we try to insert it, starting at its
**crafting eta**, into the timetable. If otherwise the considered task is **not
runnable** (even after assignin unclaimed items to it), we unassign any
previously claimed items, and then calculate a **crafting list** and a
corresponding **collector tour** through the map, collecting all *required
items w.r.t. the task's crafting list*. We then try to **donate** all
*unclaimed items* to the collector tour. The tour cannot be nonempty at this
point (otherwise the task would have been eventually runnable in the first
place).

**Donating** items to a collector tour tries to shorten a tour as much as
possible by removing waypoints. The items that would have been collected at
these waypoints are instead taken from the unclaimed items in the inventory.
However, as few as possible items are claimed from the inventory. An example:
The collector tour would visit the chest at location A, and take 300 iron from
it. Then, it would visit the chest at B, and take 100 more iron, and 200 copper
from there. We donate 340 iron and 170 copper. This is not sufficient to remove
the stop at B. However, it's sufficient to remove the stop at A. We therefore
remove stop A, and claim 300 iron to the tasks that would have been served by
this stop. Afterwards, we have 40 unclaimed iron and 170 unclaimed copper in
the inventory.

We prefer to keep a **buffer of items** in our inventory. If that buffer saves
us from having to walk somewhere to fetch items, we use it. If we must walk
there anyway, we do not use the buffer. **TODO**: *That's dumb. Better use the
buffer as far as possible, but then refill it there. Idea: a collector tour
attempts to ensure that N items of a type are there *after* these tasks have
finished. Unclaimed items are instantly assigned to tasks, in order of their
priority. After the collector task has finished, our buffers are full again.*

#### FIXME: maybe that's the solution:
Whenever the task scheduler runs, there are no claimed items (because there are
no currently running tasks).

When a task can run with what we have in our inventory (without creating debt),
it may do so. If not, then for each item that would create debt, we collect it,
with an aftermath of N items (not zero). I.e. we collect more than we need, to
re-fill our buffer. (Also, for items that would suffice, we check whether
re-filling them would cause nearly-zero cost; if so, we refill them.)

We then try to add lower-priority tasks to the collector task, if they don't
extend its original length by more than 10%.

Then, existing inventory is claimed to the tasks depending on their actual
priority. (The same is true for items that get collected.)
Yet crafts are scheduled in their order of the grocery-store-sorter.

Donations can still occur. As far as the inventory is concerned, they're
just handled like collects, i.e. they're assigned in the order of task
priorities. If they can save us from visiting a station, then we update the
path accordingly.

#### end fixme



If the
resulting timetable would be valid, the insertion is performed. If the
timetable has become **executable**, its first task is returned. Otherwise,
the next pending task is considered.

It can happen that no task can be selected.

