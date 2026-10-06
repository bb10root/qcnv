#!/bin/bash
source ~/bbndk/bbndk-env_10_3_0_698.sh
ntoarmv7-gcc -O0 -DDEBUG -std=gnu99 -fPIC -shared qc_nv_lib.c -o libqc_nv.so
ntoarmv7-gcc -O0 -DDEBUG -std=gnu99 -I ../include qcnv_tool.c \
    -L. -lqc_nv \
    -Wl,-rpath,'$ORIGIN' \
    -lc -o qcnv_tool