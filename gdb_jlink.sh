#!/bin/bash

JLINK_SERVER_PID=
GDB_PID=

function is_running {
	res= 
	kill -0 $1 2>&1 | (read res; \
		if [ -z "$res" ]; then \
			echo "true"; \
		else \
			echo "false"; \
		fi \
	)
}


function graceful_exit {
	if [ "true" == $(is_running $JLINK_SERVER_PID) ]; then
		echo "Terminating JLink Server (pid=$JLINK_SERVER_PID)";
		kill -9 $JLINK_SERVER_PID;
	fi
	if [ "true" == $(is_running $GDB_PID) ]; then
		echo "Terminating GDB (pid=$GDB_PID)";
		kill -9 $GDB_PID;
	fi
	exit
}

#trap echo "" SIGTERM
#trap echo "" SIGINT
#trap graceful_exit SIGTERM
#trap graceful_exit SIGINT

JLinkGDBServer -device $1 -speed $2 -if $3 > $4 2>&1 &
echo "$!"

#(gdb &) &
#GDB_PID= $!
#
#while true;
#graceful_exit

