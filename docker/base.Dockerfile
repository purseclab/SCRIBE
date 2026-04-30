FROM ubuntu:noble

ARG DEBIAN_FRONTEND=noninteractive

RUN dpkg --add-architecture i386
RUN apt-get update && \
    apt-get install -y \
        build-essential cmake git git-lfs graphviz lld nasm \
        libc6:i386 libgcc1:i386 libstdc++6:i386 zlib1g:i386 \
        libc6-dev-armhf-cross gcc-arm-none-eabi gcc-multilib \
        libffi-dev libglib2.0-dev libpixman-1-dev libreadline-dev \
        libsecret-1-0 libssl-dev libtool libxml2-dev libxslt1-dev \
        binutils-multiarch debootstrap debian-archive-keyring \
        gcc-avr binutils-avr avr-libc \
        python3-dev python3-pip python3-venv python-is-python3 \
        virtualenvwrapper \
        openjdk-21-jdk openjdk-17-jdk unzip wget gnupg lsb-release software-properties-common \
        qtdeclarative5-dev linux-headers-generic vim
RUN ln -s ld-linux-x86-64.so.2 /lib64/ld-lsb-x86-64.so.3
RUN apt-get install -y llvm-15 clang-15 lld-15
RUN rm -rf /var/lib/apt/lists/*

ENV VIRTUAL_ENV=/opt/venv
RUN python3 -m venv $VIRTUAL_ENV
ENV PATH="$VIRTUAL_ENV/bin:$PATH"

RUN wget -qO- https://sh.rustup.rs | sh -s -- -y
ENV PATH="/root/.cargo/bin:$PATH"
RUN pip install --upgrade pip scikit-build-core

WORKDIR /
RUN wget https://github.com/NationalSecurityAgency/ghidra/releases/download/Ghidra_11.2_build/ghidra_11.2_PUBLIC_20240926.zip && \
    unzip ghidra_11.2_PUBLIC_20240926.zip && \
    rm ghidra_11.2_PUBLIC_20240926.zip
ENV GHIDRA_INSTALL_DIR=/ghidra_11.2_PUBLIC

ARG CACHE_TIMESTAMP=1725366375
RUN git clone https://github.com/angr/angr-dev
WORKDIR /angr-dev
RUN ./setup.sh

ARG CACHE_TIMESTAMP=1714104672
RUN pip install headless-ida pyhidra

ARG CACHE_TIMESTAMP=1751908017
RUN git clone https://github.com/purseclab/patcherex2 && pip install -e patcherex2

# Recompiler LLVM passes (built via recompiler/build.sh prior to docker build).
COPY ./recompiler /root/recompiler
COPY ./recompiler/build/lib/libRecompiler.so /angr-dev/patcherex2/src/patcherex2/components/assets/llvm_recomp/libRecompiler.so
COPY ./recompiler/build/lib/libRecompilerPlugin.so /angr-dev/patcherex2/src/patcherex2/components/assets/llvm_recomp/libRecompilerPlugin.so

# IDA Pro is not bundled in the image. If you need to re-decompile a fresh
# binary (scripts/decompile_func.py / dump_asm_and_decomp.py), bind-mount your
# install at runtime, e.g.:
#   docker run -v /path/to/ida:/root/ida -v /path/to/.idapro:/root/.idapro \
#       -e DECOMPILER_CONFIG_IDA_PATH=/root/ida \
#       -e DECOMPILER_CONFIG_IDA_DEF_H_FILE_PATH=/root/ida/idapro/plugins/hexrays_sdk/include/defs.h \
#       recomp_base ...

COPY ./src /root/src
COPY ./pyproject.toml /root/pyproject.toml
RUN pip3 install -e /root

COPY ./scripts /root/scripts

WORKDIR /root
CMD ["/bin/bash"]
