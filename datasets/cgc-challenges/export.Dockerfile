ARG IMAGE_TAG=latest
FROM cgc-build:$IMAGE_TAG AS cgc-build
FROM scratch AS cgc-export
COPY --from=cgc-build /output/bin /
