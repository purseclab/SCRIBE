#!/usr/bin/env bash
# Build the SCRIBE base image. Run from the repo root or anywhere — paths are
# resolved relative to this script.
set -euo pipefail

# Pick up TAG from docker/.env if present.
if [ -f "$(dirname "$0")/.env" ]; then
    set -a
    source "$(dirname "$0")/.env"
    set +a
fi

IMAGE_TAG=${1:-${TAG:-latest}}
echo "Building recomp_base:$IMAGE_TAG"

cd "$(dirname "$0")/.."
docker build -t "recomp_base:$IMAGE_TAG" -f ./docker/base.Dockerfile .
