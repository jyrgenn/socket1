#!/bin/sh
# $Id$
#
# remote finger program using socket(1). Works similar to finger(1),
# but can do only remote fingers.

# If you have my nslook program, you can determine the official name
# of the host.
#NSLOOK="nslook -o"
NSLOOK=echo

if [ "$1" = -l ] ; then
	long=/W ; shift
fi

host=`echo $1 | sed -n 's/^.*@\([^@]*\)$/\1/p'`

if [ $# != 1 -o ! "$host" ] ; then
	echo Usage: `basename $0` '[-l] [user]@host'
	exit 1
fi

ohost=`$NSLOOK $host`
echo \[$ohost\]
echo `echo $1 | sed -n 's/^\(.*\)@[^@]*$/\1/p'` $long | socket -c $host finger
