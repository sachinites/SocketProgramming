#!/bin/bash

for (( i=0; i<$1; i++ ))
do
sed 's/define N 3/define N '$1'/g' node0.c | sed 's/define current_node_number 0/define current_node_number '$i'/g' > temp$i.c
gcc temp$i.c -o temp$i.o
chmod +x temp$i.o
gnome-terminal -e ./temp$i.o -t "Node $i"
rm temp$i.c
done
