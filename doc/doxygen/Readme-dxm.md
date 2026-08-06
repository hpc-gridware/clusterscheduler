# Documentation gate (DXM)

The source code of this project is being converted to a single documentation
format: doxygen. The files in this directory that start with `dxm-`, plus
`Doxyfile.strict`, are the tooling for that conversion.

## Why the build just failed on a comment

If a build failed with something like

```
error: Member foo(int x) (function) of file bar.cc is not documented.
```

then `bar.cc` is in a module whose documentation has already been converted, and
the gate keeps it that way. Add a doxygen block above the function:

```c
/** @brief One sentence saying what it does
 *
 * @param x what this parameter means
 * @return what comes back, and what a failure value means
 */
```

Every parameter needs a `@param`, every non-void function needs a `@return`.

Only converted modules are checked. The list is in `CMakeLists.txt`
(`DXM_FINISHED_MODULES`) and is empty until the first module lands.

## Tools

| File | Purpose |
|---|---|
| `Doxyfile` | the normal documentation build, unchanged, generates HTML and PDF for everything |
| `Doxyfile.strict` | the gate: no useful output, only converted modules, every defect is an error |
| `dxm-check.sh` | run the gate against any directory, without a cmake build tree |
| `dxm-nocomment-diff.sh` | prove a change touched only comments |
| `dxm-strip-comments.py` | helper for the above, removes comments from C/C++ |

```
cd clusterscheduler

# is this module finished?
doc/doxygen/dxm-check.sh source/libs/evc

# did I accidentally change code while editing comments?
doc/doxygen/dxm-nocomment-diff.sh source/libs/evc
```

## Message catalogues

`msg_*.h` files are exempt. Their `_MESSAGE(id, text)` entries document
themselves, so no per macro comment is wanted and the gate does not ask for one
(`EXCLUDE_PATTERNS` in `Doxyfile.strict`). The `@file` block describing the
catalogue as a whole is still worth having.

## Two things that surprise people

**A file with no `/** @file */` block is invisible to the gate.** Doxygen
reports nothing at all for such a file - not undocumented functions, not even
missing `@param` tags. Adding the `@file` block is therefore the first step when
converting a file, not a finishing touch.

**Function documentation belongs in the `.cc`, not the header.** Doxygen
attaches the block to the declaration automatically, so there is no reason to
duplicate it, and the documentation stays next to the code it describes.

## Full conventions

The complete rules - the mapping from the legacy `/****** module/func() ... */`
banner headers, comment placement, how to name parameters and constants in prose,
tables and diagrams - live outside this repository, in the project vault under
`02 Work Projects/22 Doxygen Comment Migration/`.
