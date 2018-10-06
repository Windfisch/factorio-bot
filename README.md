Factorio Bot
============

This project tries to be a **fair** bot/AI for the game
[Factorio](https://www.factorio.com) (version 0.16.51).

"Fair" means that the bot only uses information that is available to a human
player. Better planning algorithms or map memory is fine, but being able to
"see" yet-undiscovered parts of the map, or "looking through the fog of war"
is not considered "fair".

The project is in a **very early** stage, in fact, it's not much more than a
proof-of-concept that this can actually work.

Features right now:

  * we can read out the map / buildings on the map
  * most buildings are drawn, if you zoom in far enough
  * we can plan walking paths
  * we can actually walk :)
  * place entities
  * mine entities
  * craft recipes
  * inventory updates
  * action scheduler / task concept


Screenshots
-----------

![map overview](screenshots/overview.png)

![zoomed in](screenshots/zoomed.png)


Setup
------

Things are a bit fiddly right now. If you are on Linux, then there are various
helper scripts for you:

`./prepare.sh` will unpack three factorio installations (server and two
clients) and install the lua-part of the bot.

`./launch.sh --start/--stop/--run {server,Nayru,Farore,offline}` will start/stop the
corresponding instance. `--run` will start, wait for `^C` and then stop. `offline` is
special in that it launches a single-player game that can only be used together with
`--bot-offline`.

`./launch.sh --bot` will start the bot. You can also use `--bot-offline` for a
read-only version that only operates on the data file, without the need of a
running server instance.

If you're not on Linux, or if stuff doesn't work, read the internals below.


Usage
-----

Once the bot is running, you will see a map. Scroll with drag-and-drop, zoom
with mousewheel.

You can plan paths by first left-clicking the starting point, then
right-clicking the destination. Your character's location (which should
be the starting point) is denoted as a red dot on the map.

A path will show up in the map, and your Factorio player will start moving.

The `p` button switches between visualisation of the ore types and the ore-patch
ids. `s` gives info about the current scheduler state, `r` recalculates the
scheduler, `t` and `y` insert a high- or low-priority wood chopping task,
respectively. `i` gives additional info about the entity under the cursor.

For a quick demo, press (in that order):

  - `r`: the bot will walk and start chopping wood in the center of the map
  - `t`, `r`: the bot will interrupt what it was doing and chop wood in the
     north west. (After finishing, it will return to the center chopping task)
  - `y`, `r`: the bot will not interrupt what it was doing and once finished,
    will start a low priority wood chopping task in the south east.

Bugs / Limitations
------------------

Walking will appear really "jumpy" in the Factorio client which is being
"remote-controlled". A second player, following the remote-controlled one,
will not see anything jumpy.

Path planning will hang for a long time if no path can be found.

With factorio0.16, the graphical clients (i.e. Nayru etc) fail to join the game
when launched. The error message suggests that the system is too slow. A click
on the reconnect button solves the issue for now. Other than that, removing the
call to `writeout_pictures` in `control.lua` of the luamod is another solution,
because this functions takes a lot of time.

Building / Requirements
-----------------------

The bot is written in C++, using the C++17 standard. Dependencies are 
[Boost](http://boost.org) and [FLTK](http://fltk.org)-1.3.4 for the GUI. (On Ubuntu,
run: `apt-get install libboost-dev libfltk1.3-dev` to get them)

The build system used is GNU Make. So just type `make all` and you should™ be
done.

This will create two executables: `rcon-client` with the obvious job, and
`bot`, which is the bot program.

Internals
---------

How the communication between the bot and a factorio game instance works in detail
is explained [in this document](doc/factorio_comm.md).

Also, there's a detailed description of the [task scheduler](doc/scheduler.md).


License
=======

Copyright (c) 2017, 2018 Florian Jung (flo@windfisch.org)

*factorio-bot* is free software: you can redistribute it and/or
modify it under the terms of the **GNU General Public License,
version 3**, as published by the Free Software Foundation.

*factorio-bot* is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a [copy of the GNU General Public License](LICENSE)
along with factorio-bot. If not, see <http://www.gnu.org/licenses/>.

Exceptions
----------

Note that some portions of the codebase are -- where explicitly noted in the
source files -- additionally licensed under the terms of the MIT license.



See also
========

Related to this is my [production flow optimizer project](https://github.com/Windfisch/production-flow).
