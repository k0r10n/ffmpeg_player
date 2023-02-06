#!/bin/sh
CFLAGS="-g -Wall -Werror -I /usr/local/include"
CC="gcc"
LDLIBS="-lX11 -ldl -lGL -lavformat -lavcodec -lavutil -lswscale"

$CC $CFLAGS -o linux/bin/ffmpeg_player linux/ffmpeg_player.c $LDLIBS
