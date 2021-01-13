#!/bin/bash

IFS=$'\n'

FN="$2"
SCR_CONFIG=`readlink -f scripts/config`
EXPR_TOOL="/usr/bin/expr"

for line in `cat "$FN"`
do
prefix=${line:0:3}

if [ "$prefix" = "DEL" ]; then
config=${line:11}
$SCR_CONFIG --file $1 -d $config

elif [ "$prefix" = "VAL" ]; then
len=`$EXPR_TOOL length $line`
idx=`$EXPR_TOOL index $line "="`
config=`$EXPR_TOOL substr "$line" 12 $[$idx-12]`
val=`$EXPR_TOOL substr "$line" $[$idx+1] $len`
$SCR_CONFIG --file $1 --set-val $config $val

elif [ "$prefix" = "STR" ]; then
len=`$EXPR_TOOL length $line`
idx=`$EXPR_TOOL index $line "="`
config=`$EXPR_TOOL substr "$line" 12 $[$idx-12]`
str=`$EXPR_TOOL substr "$line" $[$idx+1] $len`
$SCR_CONFIG --file $1 --set-str $config $str

elif [ "$prefix" = "ADD" ]; then
config=${line:11}
$SCR_CONFIG --file $1 -e $config

elif [ "$prefix" = "MOD" ]; then
config=${line:11}
$SCR_CONFIG --file $1 -m $config

fi
done
