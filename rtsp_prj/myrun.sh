#!/bin/sh

path=/home/zhou/ffmpeg/api_learn

FFMPEG_LIB="-lavdevice -lavformat -lavfilter -lavcodec -lswresample -lswscale -lavutil"
SDL2_LIB="-lSDL2"
OTHER_LIB="-lpthread"


depend="rtsp.c rtp_h264.c rtp_aac.c rtp.c sock.c capture_h264.c packet_queue.c"
file="test_set.c"

gcc $file $depend -I $path/include/ -L $path/lib/  $FFMPEG_LIB $SDL2_LIB $OTHER_LIB
