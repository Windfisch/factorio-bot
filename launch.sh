#!/bin/bash

CONF=botconfig.conf

set -e

if [ ! -e $CONF ]; then
	echo "Error: $CONF doesn't exist. run ./prepare.sh first"
	exit 1
fi

if [ ! $# -eq 2 ] && [ ! x$1 == x--bot ] && [ ! x$1 == x--bot-offline ] && [ ! x$1 == x--newmap ] && [ ! x$1 == x--restoremap ]; then
	echo "Usage:"
	echo "    $0 --start|--stop|--run server|clientname"
	echo "    $0 --bot|--bot-offline"
	echo "    $0 --newmap|--restoremap"
	echo ""
	echo "The first form will start or stop a Factorio server/client."
	echo "--start will not block and you must --stop manually."
	echo "--run will block, and upon pressing ^C it will automatically stop."
	echo "The second form will launch the bot."
	echo "Usually, you want to do:"
	echo "  - $0 --start server"
	echo "  - ($0 --start client)"
	echo "  - $0 --start bot"
	echo "The third form will create a new map (overwriting both the current map"
	echo "and its backup), or will restore that backup respectively"
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
	--run) ;;
	--bot) ;;
	--bot-offline) ;;
	--newmap) ;;
	--restoremap) ;;
	*)
		echo "ERROR: action '$ACTION' not understood. run '$0 --help'"
		exit 1
		;;
esac


if ( [ $ACTION == --start ] || [ $ACTION == --stop ] || [ $ACTION == --run ] ) && [ ! -d "$INSTALLPATH/$TARGET" ]; then
	echo "ERROR: target '$TARGET' does not exist."
	echo -n '      available targets are '; ( cd "$INSTALLPATH" && echo */ | sed 's./..g'; )
	exit 1
fi

if [ $ACTION == --newmap ]; then
	"$INSTALLPATH/server/bin/x64/factorio" --create "$INSTALLPATH/map.zip"
	cp "$INSTALLPATH/map.zip" "$INSTALLPATH/map.zip.orig"
fi

if [ $ACTION == --restoremap ]; then
	cp "$INSTALLPATH/map.zip.orig" "$INSTALLPATH/map.zip"
fi


if [ $ACTION == --bot ]; then
	DEBUGPREFIX="$2"
	$DEBUGPREFIX ./test "$INSTALLPATH/server/script-output/output" localhost "$RCON_PORT" "$RCON_PASS"
	exit 0
fi

if [ $ACTION == --bot-offline ]; then
	DEBUGPREFIX="$2"
	$DEBUGPREFIX ./test "$INSTALLPATH/server/script-output/output"
	exit 0
fi


PIDFILE="$INSTALLPATH/$TARGET.pid"
JOINFILE="$INSTALLPATH/server/script-output/players_connected.txt"

if [ $ACTION == --start ] || [ $ACTION == --run ]; then
	if [ -e "$PIDFILE" ]; then
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

	if [ $TARGET == server ]; then
		rm -f "$INSTALLPATH/$TARGET/script-output"/*.txt
		"$INSTALLPATH/$TARGET"/bin/x64/factorio --start-server "$MAP" --rcon-port "$RCON_PORT" --rcon-password "$RCON_PASS" --server-settings "$INSTALLPATH/server-settings.json" &
		PID=$!
	else
		"$INSTALLPATH/$TARGET"/bin/x64/factorio --mp-connect localhost --disable-audio &
		PID=$!
	fi
	
	echo $PID > "$PIDFILE"

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
			if [ "$TARGET" == server ]; then
				# Execute a dummy command to silence the warning about "using commands will
				# disable achievements". If we don't do this, the first command will be lost
				./rcon-client localhost "$RCON_PORT" "$RCON_PASS" "/silent-command print('')"
				sleep 0.5
			fi
			./rcon-client localhost "$RCON_PORT" "$RCON_PASS" "/c remote.call('windfisch','whoami','$TARGET')"
			break
		fi
	done
	if [ $i == $TIMEOUT ]; then
		echo "ERROR: timeouted"
		exit 1
	fi

	if [ $ACTION == --run ]; then
		trap '' SIGINT # don't let CTRL+C kill this script, but let it kill the wait
		wait $PID
		if kill -s SIGINT $PID &>/dev/null; then
			echo
			echo '******** SIGINT ********'
			echo
			wait $PID
			echo
			echo 'Factorio has been stopped'
		else
			echo
			echo 'Factorio exited'
		fi
	fi
elif [ $ACTION == --stop ]; then
	if [ ! -e "$PIDFILE" ]; then
		echo "ERROR: pidfile '$PIDFILE' does not exist. can not stop anything"
		exit 1
	fi

	kill -s SIGINT `cat "$PIDFILE"` || echo "ERROR: pidfile '$PIDFILE' seems invalid. deleting it"
	rm "$PIDFILE"

	echo "stopped $TARGET"
fi
