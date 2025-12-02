#!/usr/bin/env bash

symbols=$(nm -Cn -t d $1 | sed -nE 's/^0*([0-9]+)\s+.\s+([^$\d].*)$/\1\2/p')
num_symbols=$(echo "$symbols" | wc -l)

cat << EOM
$num_symbols
$symbols
EOM