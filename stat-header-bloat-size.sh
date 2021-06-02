#!/bin/bash

FILE=$1

[ "$FILE" = "" ] && FILE=sched.h

ls -lS $(cat kernel/header_test_$FILE.i.headers.flat ) | less

