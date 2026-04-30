ARG IMAGE_TAG=latest
FROM coreutils-build:$IMAGE_TAG AS coreutils-build
FROM scratch AS coreutils-export
COPY --from=coreutils-build /output/bin /
