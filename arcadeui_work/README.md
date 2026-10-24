# Arcade UI — AOT prototype 0.2

Native C++ module runtime and deterministic HTML/CSS to `.arcuxi` compiler. Based on the original ArcadeUI skeleton.

## Build (Clang 23.1.3 + CMake 4.5.0-rc1 + Ninja)

```sh
cmake --preset debug
cmake --build --preset debug
ctest --test-dir build/debug --output-on-failure
./build/debug/arcadeui ./build/debug/dashboard.arcuxi
```

AOT standalone compilation: `python tools/arcuxc.py examples/dashboard.html -o dashboard.arcuxi`.

## Implemented

- HTML subset parser with `style` blocks, `id`, `class`, inline styles, `onclick` string ID.
- CSS simple selectors, cascade by specificity, numeric px values, limited Flex row/column and basic Grid columns.
- Deterministic little-endian binary v1, relative string offsets, SHA-256 payload checksum from compiler, packed assets.
- Breadth-first densely-packed topology (contiguous siblings), PMR storage, native module, layout-to-draw batch.
- Callback ID dispatch via `function pointer + context`, hit testing, responsive recomputation via `resize`.
- Python compiler unit tests and runtime smoke test.

## Limitations / next stage

Not a full browser engine: no complex CSS selectors, box sizing variations, `minmax`, intrinsic Grid tracks, CSS percentages, wrapping Flex, accessibility tree, font shaping, animations, or GPU renderer yet. Currently the runtime generates draw commands and can export an SVG preview; no window or GPU rendering. Assets are packed but aren't decoded/rendered. Coroutine input scheduler and dependency-based incremental layout are future work. SHA-256 written by compiler is not yet revalidated at runtime. Format isn't hardened for untrusted images: validate further before loading external content. Text width is approximated. Grid uses heuristic square-column auto placement. `display:none` currently does not completely remove descendants from hit-testing. Intended as foundation, not CSS specification conformance.

The original `src/app.cppm` and `src/contracts.cppm` are preserved for reference but excluded from build until reworked.

## Verification in this environment

Python unit tests (5/5) passed. The C++ module was precompiled, compiled, linked, and the example loaded using Clang 17 and `-std=c++2b` as a compatibility smoke check. Clang 23.1.3 / CMake 4.5.0-rc1 are **not installed here**, so the requested toolchain was not tested.
