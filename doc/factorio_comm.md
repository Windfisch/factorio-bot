Bridging between the Bot and Factorio
=====================================

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

