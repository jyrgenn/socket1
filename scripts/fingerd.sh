#!/bin/sh
# $Id$
#
# finger daemon program using socket(1) with primitive logging.
# Set LOGFILE to a file of your choice.

LOGFILE=/dev/null

socket -sqlvcp '/usr/ucb/finger `head -1 | sed "s|/[Ww]|-l|"`' \
		finger 2>&1 | 
	while read line ; do 
		echo -n $line:\ ; date ;
	done > $LOGFILE
