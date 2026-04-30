#!/bin/bash

# if first arg is set then export the arg as IMAGE_TAG, else use "latest"
if [ -z "$1" ]; then
  export IMAGE_TAG="latest"
else
  export IMAGE_TAG="$1"
fi

docker build -f build.Dockerfile -t cgc-build:O0 --build-arg CFLAGS="-O0" --build-arg CXXFLAGS="-O0" .
docker build -f build.Dockerfile -t cgc-build:O1 --build-arg CFLAGS="-O1 -fno-inline" --build-arg CXXFLAGS="-O1 -fno-inline" .
docker build -f build.Dockerfile -t cgc-build:O2 --build-arg CFLAGS="-O2 -fno-inline" --build-arg CXXFLAGS="-O2 -fno-inline" .
docker build -f build.Dockerfile -t cgc-build:O3 --build-arg CFLAGS="-O3 -fno-inline" --build-arg CXXFLAGS="-O3 -fno-inline" .


docker build -f export.Dockerfile -o output --build-arg IMAGE_TAG=${IMAGE_TAG} .
docker build -f export_with_symbol.Dockerfile -o output-unstripped --build-arg IMAGE_TAG=${IMAGE_TAG} .
