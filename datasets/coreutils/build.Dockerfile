FROM ubuntu:jammy AS coreutils-build

ARG DEBIAN_FRONTEND=noninteractive
ARG GIT_REPO=git://git.sv.gnu.org/coreutils
# tags/v9.4 9530a14420fc1a267e90d45e8a0d710c3668382d, line 26 disable-gcc-warning because -Werror=maybe-uninitialized
ARG GIT_COMMIT=9530a14420fc1a267e90d45e8a0d710c3668382d
ARG CFLAGS
ARG CXXFLAGS
ARG TARGET

ENV QEMU_LD_PREFIX=${TARGET:+/usr/$TARGET}

RUN cp /etc/apt/sources.list /etc/apt/sources.list~ && \
    sed -Ei 's/^# deb-src /deb-src /' /etc/apt/sources.list && \
    apt-get update && apt-get install -y \
    git rsync wget \
    ${TARGET:+gcc-$TARGET g++-$TARGET qemu-user} && \
    apt-get build-dep coreutils -y && \
    rm -rf /var/lib/apt/lists/*

RUN git clone $GIT_REPO && \
    mkdir -p /output && \
    cd `basename -s ".git" $GIT_REPO` && \
    git checkout $GIT_COMMIT && \
    ./bootstrap

RUN cd `basename -s ".git" $GIT_REPO` && \
    mkdir builddir && cd builddir && \
    FORCE_UNSAFE_CONFIGURE=1 ../configure ${TARGET:+--host=$TARGET} --disable-gcc-warnings --prefix=/output && \
    make -j $(nproc) && \
    make install

# strip binaries
RUN cp -r /output/bin /output/bin-unstripped
RUN if [ -n "$TARGET" ]; then export STRIP=$TARGET-strip; else export STRIP=strip; fi && \
    echo "Using strip command: $STRIP" && \
    find /output/bin -type f -exec $STRIP {} \;
WORKDIR coreutils/builddir
