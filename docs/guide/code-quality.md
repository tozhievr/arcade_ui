# Code quality in CI

Arcade UI keeps the existing repository-wide `.clang-format` and
`.clang-tidy` profiles. CI uses **LLVM/Clang 21** for reproducibility until
the project's preferred Clang 23 toolchain is publicly available in the
runner image.

## Formatting

The entry point is `tools/clang-format-all.py`.

```sh
python tools/clang-format-all.py --check --binary clang-format-21
python tools/clang-format-all.py --fix --binary clang-format-21
python tools/clang-format-all.py --check --changed-from origin/master --binary clang-format-21
```

It recursively covers `src/` and `tests/`, including C++ Modules `.cppm`
and `.ixx`. Output order is deterministic and nested source paths are retained.
Generated/vendor directories are excluded.

The `C++ Formatting` workflow (`.github/workflows/format.yml`) **fails**
if C++ files touched by the PR/push are not formatted. A full-repository check
also runs in report-only mode to expose existing formatting debt. At the time
this workflow was introduced, Clang Format 21 reported 17 pre-existing files
with differences; the repository had been formatted using a newer local tool,
so version-specific formatting changes are also possible.

**Automated fix:** open GitHub Actions → **C++ Formatting** → **Run workflow**,
choose the branch and set `apply=true`. The bot formats both `src/` and
`tests/`, then commits the formatting-only changes. This workflow is opt-in
and does not write to code on ordinary PR/push checks.

## Static analysis

Native CI configures/builds the entire C++ Modules tree, runs CTest, and then
executes `tools/clang-tidy-audit.py` against `build/native/compile_commands.json`.
For each translation unit it records diagnostics, source positions,
check identifiers, and clang-tidy process errors. The report is uploaded as
the `clang-tidy-audit` GitHub Actions artifact.

```sh
python tools/clang-tidy-audit.py --build build/native \
  --binary clang-tidy-21 \
  --output build/native/clang-tidy-report.json
```

**Audit policy:** there is existing warning debt. The full report is visible,
but warnings and clang-tidy nonzero exits are **not yet a merge blocker**.
Run `--strict` locally to fail on all findings. A warning-only report is not
proof that every C++ module passed semantic analysis: inspect
`nonzero_exits` for unsupported compiler/module diagnostics.

Next: establish a trusted warning baseline and fail PRs that introduce **new**
bugprone, analyzer and concurrency findings. Also validate a separate build
against Clang 23 when that runner toolchain becomes available.
