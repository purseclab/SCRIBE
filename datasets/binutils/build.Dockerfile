FROM ubuntu:jammy AS binutils-build

ARG DEBIAN_FRONTEND=noninteractive
ARG GIT_REPO=git://sourceware.org/git/binutils-gdb
# tags/binutils-2_42 c7f28aad0c99d1d2fec4e52ebfa3735d90ceb8e9
ARG GIT_COMMIT=c7f28aad0c99d1d2fec4e52ebfa3735d90ceb8e9
ARG CFLAGS
ARG CXXFLAGS
ARG TARGET

ENV QEMU_LD_PREFIX=${TARGET:+/usr/$TARGET}

RUN cp /etc/apt/sources.list /etc/apt/sources.list~ && \
    sed -Ei 's/^# deb-src /deb-src /' /etc/apt/sources.list && \
    apt-get update && apt-get install -y \
    git \
    libgmp-dev libmpc-dev libmpfr-dev \
    ${TARGET:+gcc-$TARGET g++-$TARGET qemu-user} && \
    apt-get build-dep binutils -y && \
    rm -rf /var/lib/apt/lists/*

RUN git clone $GIT_REPO && \
    mkdir -p /output && \
    cd `basename -s ".git" $GIT_REPO` && \
    git checkout $GIT_COMMIT && \
    mkdir builddir && cd builddir && \
    FORCE_UNSAFE_CONFIGURE=1 ../configure ${TARGET:+--host=$TARGET} --disable-gcc-warnings --prefix=/output \
    --disable-gdb --disable-libdecnumber --disable-readline --disable-sim --disable-gprof --disable-gdbserver --disable-gprofng --disable-gas --disable-ld && \
    make -j $(nproc) && \
    make install

# strip binaries
RUN if [ -n "$TARGET" ]; then export STRIP=$TARGET-strip; else export STRIP=strip; fi && \
    echo "Using strip command: $STRIP" && \
    find /output/bin -type f -exec $STRIP {} \;

WORKDIR /binutils-gdb/builddir
