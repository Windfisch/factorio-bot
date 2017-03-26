#!/bin/bash

CONF=botconfig.conf

set -e

if [ ! -e $CONF ]; then
	echo "Error: $CONF doesn't exist. run ./prepare.sh first"
	exit 1
fi

if [ ! $# -eq 2 ]; then
	echo "Usage:"
	echo "    $0 --start|--stop server|clientname"
	echo ""
	echo "This tool will install factorio to /installation/path/, prepare the mod and"
	echo "generate a config file into the current directory"
	exit 0
fi

ACTION=$1
TARGET=$2

source $CONF

if [ ! "${MAP:0:1}" = "/" ]; then
	MAP="$INSTALLPATH/$MAP"
fi

echo "INSTALLPATH='$INSTALLPATH'"
echo "MAP='$MAP'"

case "$ACTION" in
	--start) ;;
	--stop) ;;
	*)
		echo "ERROR: action '$ACTION' not understood. run '$0 --help'"
		exit 1
		;;
esac


if [ ! -d "$INSTALLPATH/$TARGET" ]; then
	echo "ERROR: target '$TARGET' does not exist."
	echo -n '      available targets are '; ( cd "$INSTALLPATH" && echo */ | sed 's./..g'; )
	exit 1
fi

PIDFILE="$INSTALLPATH/$TARGET.pid"
JOINFILE="$INSTALLPATH/server/script-output/players_connected.txt"

if [ $ACTION == --start ]; then
	if [ -e $PIDFILE ]; then
		if kill -0 `cat "$PIDFILE"` 2>/dev/null; then
			echo "ERROR: pidfile '$PIDFILE' already exists. refusing to start."
			echo "       if you are sure, that $TARGET isn't running already, you can remove it"
			exit 1
		else
			echo "WARNING: pidfile '$PIDFILE' exists, but contains invalid PID. deleting it"
			rm -v "$PIDFILE"
		fi
	fi

	echo "launching target '$TARGET'"
	rm -f "$JOINFILE"
	pushd "$INSTALLPATH/$TARGET/"

	if [ $TARGET == server ]; then
		./bin/x64/factorio --start-server "$MAP" --rcon-port "$RCON_PORT" --rcon-password "$RCON_PASS" --server-settings "../server-settings.json" &
		echo $! > "$PIDFILE"
	else
		./bin/x64/factorio --mp-connect localhost &
		echo $! > "$PIDFILE"
	fi
	popd

	# wait for $JOINFILE to appear and call whoami($TARGET)
	TIMEOUT=90
	for ((i=0; i < $TIMEOUT; i++)); do
		if [ $i == 30 ]; then
			echo ""
			echo "**************************************************************"
			echo "** STRANGE: client hasn't joined yet. that's unusually slow **"
			echo "**************************************************************"
			echo ""
		fi

		if [ ! -e "$JOINFILE" ]; then
			sleep 1
		else
			echo ""
			echo ""
			echo Client `cat "$JOINFILE"` "has joined while launching target '$TARGET'"
			sleep 3
			./rcon-client localhost "$RCON_PORT" "$RCON_PASS" "/c remote.call('windfisch','whoami','$TARGET')"
			break
		fi
	done
	if [ $i == $TIMEOUT ]; then
		echo "ERROR: timeouted"
		exit 1
	fi
elif [ $ACTION == --stop ]; then
	if [ ! -e $PIDFILE ]; then
		echo "ERROR: pidfile '$PIDFILE' does not exist. can not stop anything"
		exit 1
	fi

	kill -s SIGINT `cat "$PIDFILE"` || echo "ERROR: pidfile '$PIDFILE' seems invalid. deleting it"
	rm "$PIDFILE"

	echo "stopped $TARGET"
fi
