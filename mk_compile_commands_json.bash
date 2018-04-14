#!/bin/bash

TARGET=''
FILEEXTS='[^ 	]*\.\(cpp\|c\|cc\|cxx\|h\|hpp\)'
LANG=C

echo '['

PRINTED_TARGETS=' '

FIRST=1
make --trace -Bn | grep -v '\(Entering\|Leaving\) directory' | while read line; do
	if echo "$line" | grep -q -- '-MT'; then
		echo "ignoring make depend line" 1>&2
		continue
	fi

	if echo "$line" | grep -q 'update target'; then
		TMP_TARGETS="`echo "$line" | sed 's/^.*update target.*due to: \(.*\)$/\1/'`"
		TARGETS=''

		for TARGET in $TMP_TARGETS; do
			if ! echo "$TARGET" | grep -qi "$FILEEXTS"'$'; then
				echo "$TARGET is not interesting" 1>&2
			else
				TARGETS="$TARGETS $TARGET"
			fi
		done
	else
		if [ x"$TARGETS" == x ]; then
			TARGETS="`echo "$line" | grep -oi "$FILEEXTS" | tail -n1`"
			echo "no target found for line $line, let's hope that $TARGETS is right" 1>&2
		fi

		# strip off the filename: this line could be emitted for (say) a header file,
		# but with a command line for compiling a cpp file. the vim YCM plugin cannot
		# handle this, and ignores our settings for this file then. If there is no
		# contradicting compiler argument, it works.
		line=`echo "$line" | sed -e 's/\s'"$FILEEXTS"'//g'`
		escaped="`echo "$line" | sed -e 's/\\\\/\\\\\\\\/g' -e 's/"/\\\\"/g'`"
		escpwd="`pwd | sed -e 's/\\\\/\\\\\\\\/g' -e 's/"/\\\\"/g'`"

		for TARGET in $TARGETS; do
			if ! echo "$PRINTED_TARGETS" | grep -q "$TARGET"; then
				if [ x"$TARGET" != x ]; then
					if [ $FIRST == 1 ]; then
						FIRST=0
					else
						echo ','
					fi

					echo    '  { "directory": "'"$escpwd"'",'
					echo    '    "command": "'"$escaped"'",'
					echo -n '    "file": "'"$TARGET"'" }'

					PRINTED_TARGETS="$PRINTED_TARGETS $TARGET"
				fi
			else
				echo "skipping $TARGET" 1>&2
			fi
		done
	fi
done

echo
echo ']'
