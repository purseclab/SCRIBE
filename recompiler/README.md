# recompiler/

LLVM 15 components used by SCRIBE. Two pieces:

1. **Out-of-tree LLVM passes and Clang plugins** — built by `build.sh` from `src/`.
2. **A patched clang-15** — produced by applying `clang-15.diff` on top of the
   official `llvm-project` sources and rebuilding clang.

Only the passes are required to run the pipeline. The patched clang-15 is
optional and improves SCRIBE's handling of return-type mismatches; the rest
of the pipeline works against the system `clang-15` (`apt install clang-15`).

## Layout

```
recompiler/
├── src/
│   ├── CMakeLists.txt
│   ├── Misc/                # MIR-level helpers (StackToReg, ...)
│   ├── Passes/              # LLVM IR passes
│   └── Plugins/             # Clang plugins (CorrectBoolType, RegExprToInlineAsm)
├── build.sh                 # builds all of the above into build/lib/*.so
├── clang-15.diff            # Sema/SemaCast.cpp patch for the optional clang
└── README.md
```

## Build the LLVM passes

Requires LLVM 15 development headers and CMake.

```bash
sudo apt install -y llvm-15-dev libclang-15-dev clang-15 cmake
./build.sh
```

This produces:

- `build/lib/libRecompiler.so`  — out-of-tree opt/llc passes
- `build/lib/libRecompilerPlugin.so` — Clang plugins (`-Xclang -plugin
  correct-bool-type`, `-Xclang -plugin reg-to-asm`)

Both `.so` files are picked up by `docker/base.Dockerfile`, which copies them
into `patcherex2`'s `llvm_recomp` asset directory.

## (Optional) Build the patched clang-15

```bash
git clone --depth 1 --branch llvmorg-15.0.7 https://github.com/llvm/llvm-project
cd llvm-project
git apply /path/to/recompiler/clang-15.diff
cmake -S llvm -B build -G Ninja \
    -DLLVM_ENABLE_PROJECTS="clang;lld" \
    -DCMAKE_BUILD_TYPE=Release
cmake --build build --target clang
```

Drop the resulting `build/bin/clang-15` somewhere on `PATH` ahead of the
system clang, or replace `/usr/lib/llvm-15/bin/clang` inside the Docker
image (see the commented-out `COPY` in `docker/base.Dockerfile`).
