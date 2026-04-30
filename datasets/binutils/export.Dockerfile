ARG IMAGE_TAG=latest
FROM binutils-build:$IMAGE_TAG AS binutils-build
FROM scratch AS binutils-export
COPY --from=binutils-build /output/bin /
