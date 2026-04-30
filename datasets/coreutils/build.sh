#!/bin/bash

# if first arg is set then export the arg as IMAGE_TAG, else use "latest"
if [ -z "$1" ]; then
  export IMAGE_TAG="latest"
else
  export IMAGE_TAG="$1"
fi

docker build -f build.Dockerfile -t coreutils-build:O0 --build-arg CFLAGS="-O0" --build-arg CXXFLAGS="-O0" .
docker build -f build.Dockerfile -t coreutils-build:O1 --build-arg CFLAGS="-O1 -fno-inline" --build-arg CXXFLAGS="-O1 -fno-inline" .
docker build -f build.Dockerfile -t coreutils-build:O2 --build-arg CFLAGS="-O2 -fno-inline" --build-arg CXXFLAGS="-O2 -fno-inline" .
docker build -f build.Dockerfile -t coreutils-build:O3 --build-arg CFLAGS="-O3 -fno-inline" --build-arg CXXFLAGS="-O3 -fno-inline" .

docker build -f build.Dockerfile -t coreutils-build:arm32_O0 --build-arg CFLAGS="-O0" --build-arg CXXFLAGS="-O0" --build-arg TARGET="arm-linux-gnueabi" .
docker build -f build.Dockerfile -t coreutils-build:arm32_O1 --build-arg CFLAGS="-O1 -fno-inline" --build-arg CXXFLAGS="-O1 -fno-inline" --build-arg TARGET="arm-linux-gnueabi" .
docker build -f build.Dockerfile -t coreutils-build:arm32_O2 --build-arg CFLAGS="-O2 -fno-inline" --build-arg CXXFLAGS="-O2 -fno-inline" --build-arg TARGET="arm-linux-gnueabi" .
docker build -f build.Dockerfile -t coreutils-build:arm32_O3 --build-arg CFLAGS="-O3 -fno-inline" --build-arg CXXFLAGS="-O3 -fno-inline" --build-arg TARGET="arm-linux-gnueabi" .

# docker build -f export.Dockerfile -o output/latest --build-arg IMAGE_TAG="latest" .
# docker build -f export.Dockerfile -o output/O2 --build-arg IMAGE_TAG="O2" .
# docker build -f export.Dockerfile -o output --build-arg IMAGE_TAG=${IMAGE_TAG} .
# docker build -f export_with_symbol.Dockerfile -o output-unstripped --build-arg IMAGE_TAG=${IMAGE_TAG} .
# docker run -it --rm -v `pwd`/output/ls:/coreutils/builddir/src/cat:ro coreutils-build make check -j $(nproc)
