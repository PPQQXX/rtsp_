#!/bin/sh

path=/home/zhou/ffmpeg/api_learn
file=$1
depend="audio_video_thread.c stream.c packet_queue.c"

FFMPEG_LIB="-lavdevice -lavformat -lavfilter -lavcodec -lswresample -lswscale -lavutil"
SDL2_LIB="-lSDL2"
OTHER_LIB="-lpthread"


gcc $file $depend -I $path/include/ -L $path/lib/  $FFMPEG_LIB $SDL2_LIB $OTHER_LIB
