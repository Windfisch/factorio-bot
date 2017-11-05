Factorio Bot
============

This project tries to be a **fair** bot/AI for the game
[Factorio](https://www.factorio.com) (version 0.14.22).

"Fair" means that the bot only uses information that is available to a human
player. Better planning algorithms or map memory is fine, but being able to
"see" yet-undiscovered parts of the map, or "looking through the fog of war"
is not considered "fair".

The project is in a **very early** stage, in fact, it's not much more than a
proof-of-concept that this can actually work.

Features right now:

  * we can read out the map / buildings on the map
  * we can plan walking paths
  * we can actually walk :)



Setup
------

Things are a bit fiddly right now. If you are on Linux, then there are various
helper scripts for you:

`./prepare.sh` will unpack three factorio installations (server and two
clients) and install the lua-part of the bot.

`./launch.sh --start/--stop {server,Nayru,Farore}` will start/stop the
corresponding instance.

`./launch.sh --bot` will start the bot.

If you're not on Linux, or if stuff doesn't work, read the internals below.


Usage
-----

Once the bot is running, you will see a map. Scroll with drag-and-drop, zoom
with mousewheel.

You can plan paths by first left-clicking the starting point, then
right-clicking the destination. Your character's location (which should
be the starting point) is denoted as a red dot on the map.

A path will show up in the map, and your Factorio player will start moving.

<details>
<summary>Some caveats</summary>
Note that only `game.players[1]` will move, i.e. the **first** player that has
ever joined the map since creation.

Note that drag-and-dropping the map will count as a start-selecting left
click. Also note that, when clicking a tile, you actually select its **top
left corner** as a destination/starting point.

Also note that the path shown is no longer the path the player actually walks.
This is because walking now creates a `WalkTo` goal, which internally plans
its own path, starting from the player's current position.
</details>



Bugs / Limitations
------------------

Walking will appear really "jumpy" in the Factorio client which is being
"remote-controlled". A second player, following the remote-controlled one,
will not see anything jumpy.

Path planning will hang for a long time if no path can be found.

Building / Requirements
-----------------------

Most code is written in C++, using the C++14 standard. Dependencies are 
[Boost](http://boost.org) and [FLTK](http://fltk.org) for the GUI. (On Ubuntu,
run: `apt-get install libboost-dev libfltk1.3-dev` to get them)

The build system used is Make. So just type `make all` and you should™ be
done.

This will create two executables: `rcon-client` with the obvious job, and
`test`, which is the bot program.



Internals
---------

Interacting with Factorio isn't too easy. Factorio is [moddable](http://lua-api.factorio.com)
with [Lua](https://www.lua.org) (good), but due to how it's working
internally, a lot of I/O functions are disabled (bad).

We're basically forced to do the following:

  * have the Lua mod write out all information into a file under
    `factorio/script-output/`,
  * parse this file, do things, be artifically intelligent,
  * and then establish a
    [RCON](https://developer.valvesoftware.com/wiki/Source_RCON_Protocol)
    connection to Factorio and issue our commands

Additionally, gathering all map information is a quite CPU-intensive task, so
we wish to do this only on the server (since the clients won't write out the
datafile, there's no need for them to figure out what to write in the first
place). This means that we have to do **different** behaviour on different
Factorio nodes, which isn't intended in Factorio.

The way we're doing this is: After starting of a server/client, we tell the
game via RCON who has just joined. The nodes can then figure out whether it
was themselves who just joined, and thus remember their own name.  This
finally enables us to writeout only on the server.

The start up procedure is as follows:

  * start the server
  * the server will write "server" into its `script-output/players_connected.txt`
  * send `/c remote.call('windfisch','whoami','server')` via RCON
  * now the server knows that it is the server
  * start zero or more clients. the server will write "clientname" into
    `players_connected.txt`. Send `/c remote.call(...)` with the appropriate
    name.
  * start the bot (`./test`), telling it where the `script-output/output1.txt`
    file is.
  * the bot will read the file, show a GUI, and RCON your commands into the
    server.

Note that the bot actually accepts the file **prefix**, i.e.
`script-output/output`. (This is because, in a later version, we will rotate
through multiple output files, in order to limit file size.)


See also
========

Related to this is my [production flow optimizer project](https://github.com/Windfisch/production-flow).
