#!/bin/bash

CONF=botconfig.conf

set -e

if [ ! $# -eq 2 ]; then
	echo "Usage:"
	echo "    $0 path/to/factorio_alpha_x64_0.14.22.tar.gz /installation/path"
	echo ""
	echo "This tool will install factorio to /installation/path/, prepare the mod and"
	echo "generate a config file into the current directory"
	exit 0
fi

if [ -e $CONF ]; then
	echo "Error: $CONF already exists, refusing to prepare anything."
	exit 1
fi

FACTORIO_TGZ="$1"
INSTALLPATH="$2"

mkdir -p "$INSTALLPATH"


# unpack factorio
mkdir -p "$INSTALLPATH"
tar -x -v -C "$INSTALLPATH" -f "$FACTORIO_TGZ"

# install the mod
mkdir -p "$INSTALLPATH/factorio/mods"
ln -sr "./luamod/Windfisch_0.0.1" "$INSTALLPATH/factorio/mods/"
cat > "$INSTALLPATH/factorio/mods/mod-list.json" << EOF
{
    "mods": [
        {
            "name": "base",
            "enabled": "true"
        },
        {
            "name": "Windfisch",
            "enabled": "true"
        }
    ]
}
EOF


# server installation
mv "$INSTALLPATH/factorio" "$INSTALLPATH/server"

cat > "$INSTALLPATH/server-settings.json" << EOF
{
  "name": "Bot Test Game",
  "description": "A game for testing the bot",
  "tags": ["ai", "bot"],

  "_comment_max_players": "Maximum number of players allowed, admins can join even a full server. 0 means unlimited.",
  "max_players": 0,

  "_comment_visibility": ["public: Game will be published on the official Factorio matching server",
                          "lan: Game will be broadcast on LAN"],
  "visibility":
  {
    "public": false,
    "lan": false
  },

  "_comment_credentials": "Your factorio.com login credentials. Required for games with visibility public",
  "username": "",
  "password": "",

  "_comment_token": "Authentication token. May be used instead of 'password' above.",
  "token": "",

  "game_password": "",

  "_comment_require_user_verification": "When set to true, the server will only allow clients that have a valid Factorio.com account",
  "require_user_verification": false,

  "_comment_max_upload_in_kilobytes_per_second" : "optional, default value is 0. 0 means unlimited.",
  "max_upload_in_kilobytes_per_second": 0,

  "_comment_minimum_latency_in_ticks": "optional one tick is 16ms in default speed, default value is 0. 0 means no minimum.",
  "minimum_latency_in_ticks": 0,

  "_comment_ignore_player_limit_for_returning_players": "Players that played on this map already can join even when the max player limit was reached.",
  "ignore_player_limit_for_returning_players": false,

  "_comment_allow_commands": "possible values are, true, false and admins-only",
  "allow_commands": "true",

  "_comment_autosave_interval": "Autosave interval in minutes",
  "autosave_interval": 10,

  "_comment_autosave_slots": "server autosave slots, it is cycled through when the server autosaves.",
  "autosave_slots": 5,

  "_comment_afk_autokick_interval": "How many minutes until someone is kicked when doing nothing, 0 for never.",
  "afk_autokick_interval": 0,

  "_comment_auto_pause": "Whether should the server be paused when no players are present.",
  "auto_pause": false,

  "only_admins_can_pause_the_game": false,

  "_comment_autosave_only_on_server": "Whether autosaves should be saved only on server or also on all connected clients. Default is true.",
  "autosave_only_on_server": true,

  "_comment_admins": "List of case insensitive usernames, that will be promoted immediately",
  "admins": []
}
EOF

# the client directories are just hardlinked copies to save disk space
# create a config for them with a different name
for client in Nayru Farore; do
	cp -rl "$INSTALLPATH/server" "$INSTALLPATH/$client"
	cat > "$INSTALLPATH/$client/player-data.json" << EOF
{
    "latest-multiplayer-connections": [
        {
            "address": "localhost"
        }
    ],
    "service-username": "$client",
    "service-token": ""
}
EOF
done

cat > $CONF << EOF
INSTALLPATH='$INSTALLPATH'
RCON_PORT=1234
RCON_PASS=rcon123
MAP=FIXME

# MAP is either an absolute path, or relative to INSTALLPATH
EOF

echo
echo
echo "installed factorio to '$INSTALLPATH/'."
echo "created config file '$CONF'."
