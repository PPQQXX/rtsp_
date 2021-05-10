#!/bin/sh

path=/home/zhou/ffmpeg/api_learn
file=$1

gcc $file -I $path/include/ -L $path/lib/ -lavdevice -lavformat -lavfilter -lavcodec -lswresample -lswscale -lavutil
