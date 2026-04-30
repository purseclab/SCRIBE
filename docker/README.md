# docker/

The SCRIBE toolchain image — used to run `c_patch.py` for any case.

Each case under `cases/` has its own self-contained `Dockerfile` for building
the vulnerable input binary and running the upstream test suite (the
`*-export` and `*-test` targets). This directory only provides the runtime
image that hosts patcherex2, the recompiler LLVM passes, Ghidra, angr, and
the `scribe` Python package.

## Files

- `base.Dockerfile` — the toolchain image. Built as `recomp_base:${TAG:-latest}`.
- `build.sh` — convenience wrapper around `docker build`.
- `docker-compose.yaml` — single `base` service for an interactive shell.
- `.env.example` — copy to `.env` (only `TAG` and the optional IDA paths).

## Build

```bash
# 1. Build the LLVM passes on the host (one-time).
( cd recompiler && ./build.sh )

# 2. Build the image.
cp docker/.env.example docker/.env
./docker/build.sh
```

## Run a case

```bash
cd cases/coreutils/CVE-2014-9471

# Build the vulnerable input binary deterministically.
./Dockerfile                           # uses the case's own Dockerfile

# Apply the SCRIBE patch (produces date_c_patched).
docker run --rm -v "$(pwd):/workdir" -w /workdir recomp_base \
    python3 c_patch.py

# Verify the PoC no longer crashes.
./patched.sh

# Optional: run the upstream test suite (uses the case's Dockerfile again).
./test.sh
```

## IDA Pro (optional, only for re-decompilation)

IDA Pro is **not bundled** in the image. You only need it if you plan to run
`scripts/decompile_func.py` or `scripts/dump_asm_and_decomp.py` to regenerate
a `from_decompiler/` snapshot from a fresh binary. Cases ship with their
snapshots already, so most users never need IDA.

When you do, bind-mount your install at runtime:

```bash
docker run --rm \
    -v /path/to/ida:/root/ida \
    -v /path/to/.idapro:/root/.idapro \
    -e DECOMPILER_CONFIG_IDA_PATH=/root/ida \
    -e DECOMPILER_CONFIG_IDA_DEF_H_FILE_PATH=/root/ida/idapro/plugins/hexrays_sdk/include/defs.h \
    -v "$(pwd):/workdir" -w /workdir \
    recomp_base \
    python3 /root/scripts/decompile_func.py ./readelf 0xae90
```
