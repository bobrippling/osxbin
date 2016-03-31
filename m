#!/bin/sh

if test $# -ne 1
then
	mpc "$@"
else
	case "$1" in
		p|t) mpc -q toggle ;;
		s) mpc -q stop ;;
		n) mpc -q next ;;
		*) mpc "$@" ;;
	esac
fi
