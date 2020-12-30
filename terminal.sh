#!/bin/bash

PORT=/dev/ttyACM0
BAUD=9600
stty -F $PORT $BAUD
tail -f $PORT
