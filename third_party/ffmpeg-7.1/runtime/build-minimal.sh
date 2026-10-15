#!/usr/bin/env bash
set -eu
export PATH=/mingw64/bin:/usr/bin
export LC_ALL=C
cd ffmpeg-7.1
./configure \
  --prefix=/ffmpeg-runtime \
  --target-os=mingw32 --arch=x86_64 \
  --cc=gcc --cxx=g++ --ar=ar --ranlib=ranlib --strip=strip \
  --disable-autodetect --disable-everything \
  --disable-network --disable-gpl --disable-nonfree \
  --disable-doc --disable-debug --disable-x86asm \
  --disable-shared --enable-static --extra-ldflags=-static \
  --disable-avdevice --disable-postproc \
  --disable-programs --enable-ffmpeg \
  --enable-decoder=rawvideo --enable-encoder=rawvideo \
  --enable-demuxer=rawvideo --enable-muxer=rawvideo \
  --enable-protocol=pipe \
  --enable-filter=deblock,format,scale,buffer,buffersink \
  > ../configure.log 2>&1
make -j4 ffmpeg.exe > ../build.log 2>&1
