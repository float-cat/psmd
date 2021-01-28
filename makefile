#!/bin/bash

GCC='gcc'
DEL='rm -f'
C90='-std=gnu90 -pedantic'
WRN='-Wall -Wextra'
LIBS='-lm -lpthread'

$GCC -c include/*.c $C90 $WRN
$GCC main.c *.o -o psmd $C90 $WRN $LIBS
$DEL *.o
