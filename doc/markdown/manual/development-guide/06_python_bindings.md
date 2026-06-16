# Python Bindings and Build-Host Setup

xxQS_NAMExx ships an optional Python binding, the `bridge` extension module,
which exposes the event mirror and selected GDI operations to Python. It is built
with [pybind11](https://pybind11.readthedocs.io/) as part of the GCS extensions
build and is consumed by the `gcs-python-api` package (the importable `ocs`
module) and the `gcs-qmon` monitoring GUI.

The binding is only built when Python support is enabled (`WITH_PYTHON`) and the
GCS extensions are part of the build (see *Prepare the Build Configuration*,
`PROJECT_EXTENSIONS`).

## Enabling the Python Binding

Add `-DWITH_PYTHON=ON` to the `cmake` configuration. The build then requires a
Python 3 interpreter with its development headers and pybind11 (see below). The
module is produced with the interpreter's native extension suffix, e.g.
`bridge.cpython-313-x86_64-linux-gnu.so`, and installed under
*\$SGE\_ROOT/lib/\<arch\>/python/ocs/*.

## Build Prerequisites

For each Python version the binding should be built for, the build host needs:

* the **Python 3 interpreter** of that version, and
* its **development headers** (`Python.h`).

The Python *library* (`libpython`) is **not** linked — extension modules resolve
the Python C-API symbols at import time — so only the headers are required.

In addition, install **pybind11** (and optionally `pybind11-stubgen`, used to
generate the `.pyi` type stub consumed by IDEs) for the interpreter that drives
the build:

```
pip install pybind11 pybind11-stubgen
```

### Distribution packages (single version)

The simplest case is to build for the one Python version shipped by the OS:

```
# Debian / Ubuntu
apt install python3 python3-dev

# RHEL / CentOS / Rocky
dnf install python3 python3-devel
```

A distribution usually packages only one Python minor per release, so this does
not cover the case where the binding must support several Python versions (see
*Setting Up a Build Host for Multiple Python Versions*).

## Why the Binding Is Python-Version-Specific

pybind11 cannot be compiled against the Python *stable ABI* (abi3): it uses parts
of the C-API that are excluded from the limited API. A given `bridge` module
is therefore tied to one CPython *minor* (3.13, 3.14, ...). To serve several
versions from one xxQS_NAMExx installation, the binding is **built once per
minor**, and each artifact is named with that interpreter's extension suffix.
CPython's import machinery then loads only the file matching the importing
interpreter and ignores the rest, so all versions coexist in the same directory.

### Supported Python versions

The lowest supported minor is determined by pybind11: the bundled pybind11 (3.x)
requires **Python 3.8 or newer** (pybind11 2.13 was the last to support Python
3.7). Builds for 3.13, 3.14, and 3.15 are known to compile and import. Select the
set of minors to build via `OCS_BRIDGE_PYTHONS` (see *Building for Several Python
Versions*).

## Setting Up a Build Host for Multiple Python Versions

To build for several minors, each interpreter (with its headers) must be present
on the build host. As distribution packages provide only one, use one of the
following.

### uv (recommended)

[`uv`](https://docs.astral.sh/uv/) downloads self-contained CPython builds that
include the development headers — the quickest way to provision several versions
on one host:

```
uv python install 3.13 3.14 3.15
# interpreters are placed under ~/.local/share/uv/python/cpython-3.XX-.../bin/
uv python find 3.14        # prints the path of an installed interpreter
```

### pyenv (from source)

[`pyenv`](https://github.com/pyenv/pyenv) builds CPython from source (headers
included). It needs the usual CPython build dependencies (openssl, zlib, libffi,
readline, ...):

```
pyenv install 3.13 3.14 3.15
# -> ~/.pyenv/versions/3.XX.Y/bin/python
```

### manylinux container (CI / release)

For reproducible release builds, the PyPA `manylinux` images ship every CPython
under */opt/python/cp3XX-cp3XX/* with headers. This is the recommended route for
a release pipeline, at the cost of providing the full xxQS_NAMExx toolchain
inside the container.

## Building for Several Python Versions

The binding's target list is controlled by the `OCS_BRIDGE_PYTHONS` cache
variable — a `;`-separated list of interpreter executables. When unset it
defaults to the single interpreter `cmake` found, so an ordinary build is
unchanged.

To build for several minors, pass their interpreter paths:

```
cmake ... -DWITH_PYTHON=ON \
   -DOCS_BRIDGE_PYTHONS="/usr/bin/python3.13;$HOME/.local/share/uv/python/cpython-3.14-linux-x86_64-gnu/bin/python3.14;$HOME/.local/bin/python3.15"
```

The build then produces one module per version into
*\$SGE\_ROOT/lib/\<arch\>/python/ocs/*:

```
bridge.cpython-313-x86_64-linux-gnu.so
bridge.cpython-314-x86_64-linux-gnu.so
bridge.cpython-315-x86_64-linux-gnu.so
```

`make bridge` builds all configured versions; each version also has its own
target (`bridge_cpython-3XX-...`) for building individually.

The xxQS_NAMExx C++ libraries are version-independent and built once; only the
small binding layer is recompiled per Python version, so the additional cost is
low.

## Import Resolution

The `ocs` package (in `gcs-python-api`) adds *\$SGE\_ROOT/lib/\<arch\>/python/ocs/*
to its import path at run time. CPython then selects the `bridge` file whose
suffix matches the running interpreter, so `import ocs.bridge` resolves to the
build for the active Python version with no further configuration. Every build is suffix-tagged.

## Allocator Note

The binding is linked **without** the jemalloc allocator the daemons use, so it
defers to the host interpreter's allocator. Linking jemalloc into a module loaded
by a foreign interpreter mixes allocators — memory allocated via jemalloc but
freed via the host's libc — and aborts with `free(): invalid pointer`. The
binding therefore links only the shared system libraries.

## Consuming Repositories

* **`gcs-python-api`** — the importable `ocs` package, its test suite, and the
  build-host Python pin; managed with `uv` (see that repository's README).
* **`gcs-qmon`** — a PySide6 monitoring GUI built on the event mirror the binding
  exposes.


