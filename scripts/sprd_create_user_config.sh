#!/bin/bash

FN="$2"
SCR_PATH=`dirname $0`

for line in `cat "$FN"`
do
prefix=${line:0:3}

if [ "$prefix" = "DEL" ]; then
config=${line:11}
./$SCR_PATH/config --file $1 -d $config

elif [ "$prefix" = "VAL" ]; then
len=`expr length $line`
idx=`expr index $line "="`
config=`expr substr "$line" 12 $[$idx-12]`
val=`expr substr "$line" $[$idx+1] $len`
./$SCR_PATH/config --file $1 --set-val $config $val

elif [ "$prefix" = "STR" ]; then
len=`expr length $line`
idx=`expr index $line "="`
config=`expr substr "$line" 12 $[$idx-12]`
str=`expr substr "$line" $[$idx+1] $len`
./$SCR_PATH/config --file $1 --set-str $config $str

elif [ "$prefix" = "ADD" ]; then
config=${line:11}
./$SCR_PATH/config --file $1 -e $config

elif [ "$prefix" = "MOD" ]; then
config=${line:11}
./$SCR_PATH/config --file $1 -m $config

fi
done
