#!/bin/bash

# if first arg is set then export the arg as IMAGE_TAG, else use "latest"
if [ -z "$1" ]; then
  export IMAGE_TAG="latest"
else
  export IMAGE_TAG="$1"
fi

docker build -f build.Dockerfile -t binutils-build:O0 --build-arg CFLAGS="-O0" --build-arg CXXFLAGS="-O0" .
docker build -f build.Dockerfile -t binutils-build:O2 --build-arg CFLAGS="-O2 -fno-inline" --build-arg CXXFLAGS="-O2 -fno-inline" .
# DOCKER_BUILDKIT=1 docker build -f build.Dockerfile -t binutils-build:arm32_O2 --build-arg CFLAGS="-O2" --build-arg CXXFLAGS="-O2" --build-arg TARGET="arm-linux-gnueabi" .

docker build -f export.Dockerfile -o output --build-arg IMAGE_TAG=${IMAGE_TAG} .
# docker run -it --rm -v `pwd`/output/ls:/binutils/builddir/src/cat:ro binutils-build make check -j $(nproc)
