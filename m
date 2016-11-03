#!/bin/sh

if test $# -eq 0
then
	exec mpc
	exit 127
fi

arg="$1"
shift

case "$arg" in
	p)
		if test $# -ne 0
		then mpc -q play "$@"
		else mpc -q toggle
		fi
		;;
	s) mpc -q stop "$@" ;;
	n) mpc -q next "$@" ;;
	*) mpc "$arg" "$@" ;;
esac
