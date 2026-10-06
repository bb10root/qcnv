#!/bin/bash
source ~/bbndk/bbndk-env_10_3_0_698.sh

# 1. Збираємо об'єктний файл нашої бібліотеки
ntoarmv7-gcc -O0 -DDEBUG -std=gnu99 -fPIC -c qc_nv_lib.c -o qc_nv_lib.o

# 2. Створюємо статичний архів
ntoarmv7-ar rcs libqc_nv.a qc_nv_lib.o

# 3. Лінкуємо утиліту з правильними шляхами пошуку
ntoarmv7-gcc -O0 -DDEBUG -std=gnu99 -I ../include qcnv_tool.c \
    ./libqc_nv.a \
    -Wl,-rpath=/base/usr/lib:/base/lib:/base/lib/dll:/radio/lib:/radio/lib/dll:/radio/usr/lib:/proc/boot:. \
    -Wl,-export-dynamic \
    -o qcnv_tool