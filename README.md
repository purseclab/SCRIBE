# ✍️ SCRIBE Can Recompile and Inject Binary Edits

This repository contains the codebase accompanying the SCRIBE paper.

## Layout

```
.
├── pyproject.toml          # Python package config (src/ layout)
├── src/scribe/             # Decompiler interfaces (IDA, Ghidra, angr) used
│                           #   by the helper scripts
├── recompiler/             # LLVM 15 passes and Clang plugins, plus an
│                           #   optional clang-15 patch (clang-15.diff)
├── cases/                  # CVE cases. Each case is self-contained: a
│                           #   from_decompiler/ snapshot, a c_patch.py + test.sh,
│                           #   and a Dockerfile that builds the vulnerable
│                           #   binary from a pinned upstream commit.
├── datasets/               # Dockerfiles + helper scripts for building the
│                           #   full upstream binutils / coreutils binaries,
│                           #   and the CGC challenge corpus (with libcgc /
│                           #   libpov / libtiny-AES sources).
├── scripts/                # Standalone helpers: decompile a function,
│                           #   dump asm+decomp, run the LLM patcher
└── docker/                 # The recomp_base toolchain image (patcherex2,
                            #   LLVM passes, decompilers, scribe pkg)
```

## Quick start

```bash
# 1. Build the LLVM passes (host-side, needs llvm-15-dev + cmake)
( cd recompiler && ./build.sh )

# 2. Configure docker
cp docker/.env.example docker/.env
$EDITOR docker/.env                   # adjust TAG and IDA paths if you'll re-decompile

# 3. Build the toolchain image
./docker/build.sh
```

IDA Pro is **not bundled** in the image. You only need it if you plan to run
`scripts/decompile_func.py` or `scripts/dump_asm_and_decomp.py` to regenerate
a `from_decompiler/` snapshot. When you do, bind-mount your install at
runtime — see `docker/README.md`.

## Running a case

Each case under `cases/<project>/<CVE>/` is self-contained:

```
cases/coreutils/CVE-2014-9471/
├── Dockerfile              # builds the vulnerable binary from a pinned
│                           #   upstream commit (and runs the test suite)
├── from_decompiler/        # decompiler output (decompiled.c, symbols.json, ...)
├── c_patch.py              # uses patcherex2 to recompile and apply the patch
├── manual_patched.c        # reference: the manually patched C source
├── llm_patched.c           # reference: the LLM-generated patched C source
├── poc                     # crash trigger
├── crash.sh                # runs the original binary against the PoC
├── patched.sh              # runs the patched binary against the PoC
├── test.sh                 # full upstream test-suite check on the patched binary
└── src_patch.diff          # upstream's official source patch (for comparison)
```

The vulnerable input binary (e.g. `date`, `readelf`, `nm`) is **not** committed —
the case `Dockerfile` builds it deterministically from a pinned git commit.

```bash
cd cases/coreutils/CVE-2014-9471

# 1. Build the vulnerable input binary into the case dir.
./Dockerfile

# 2. Apply the SCRIBE patch (produces e.g. date_c_patched).
docker run --rm -v "$(pwd):/workdir" -w /workdir recomp_base \
    python3 c_patch.py

# 3. Verify the PoC no longer crashes.
./patched.sh

# 4. Optional: run the upstream test suite on the patched binary.
./test.sh
```

## Standalone helper scripts

See `scripts/README.md`. Most useful:

- `scripts/decompile_func.py` — regenerate a `from_decompiler/` snapshot
  for a fresh binary using IDA Pro.
- `scripts/llm_patch.py` — drive an LLM to apply SEARCH/REPLACE patches
  against a case. Needs `OPENAI_API_KEY` in the environment.

You can also `pip install -e .` from the repo root to use the `scribe`
package (decompiler interfaces) directly.

## Building dataset binaries

Each case ships with a self-contained `Dockerfile` that builds the
*specific vulnerable* upstream commit. If you instead want to build the
full upstream binaries for benchmarking or building a fresh evaluation
corpus:

```bash
cd datasets/binutils       # or coreutils / cgc-challenges
./build.sh                 # produces e.g. binutils-build:O0
```

## Citation

```bibtex
@inproceedings{scribe,
  author    = {Dai, Han and Priyadarshan, Soumyakant and Imran, Abdullah and Wang, Ruoyu and Bianchi, Antonio},
  booktitle = {2026 11th IEEE European Symposium on Security and Privacy (EuroS\&P)},
  title     = {SCRIBE: Practical Static Binary Patching via Binary-Aware Recompilation of Decompiled Code},
  year      = {2026}
}
```

## License

MIT. See `LICENSE`.
