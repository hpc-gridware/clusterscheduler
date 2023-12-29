# Building with cmake

## Prerequisits

* gcc >= 4.8.5 (e.g. default on CentOS 7 i386)
* g++ >= @todo will the 4.8.5 be new enough? Or better compile a newer one on our build host? It has only experimental support for C++11!
* libstdc++ >= @todo what version do we want / require? Might also depend on 3rdparty tools.
* cmake >= 3.0
* autoconf, automake
* git
* for building man pages and documentation pandoc, TeX and related tools are required. On Ubuntu install the packages
  * pandoc
  * texlive
  * texlive-xetex

## Building Gridengine

### Configure the Project

#### Pointing cmake to the source directory
Create a local build directory and cd into it.

Use the cmake -S option to point it to the gridengine repository (the toplevel directory)

```shell
cmake -S path_to_gridengine_directory
```

If private project extensions shall be built as well then specify the path to their root directory.
This root directory must contain a global CMakeLists.txt file for building the extensions. It can rely
on all cmake settings from the gridengine CMakeLists.txt file and use all libraries created during the
gridengine build.

```Shell
cmake -S path_to_gridengine_directory -DPROJECT_EXTENSIONS=path_to_extensions_directory
```

#### Setting the installation prefix

If you want to install to a different location than the default (`/opt/ge`) specify
`CMAKE_INSTALL_PREFIX`:

```shell
cmake -S <path_to_gridengine_directory> -DCMAKE_INSTALL_PREFIX=<install_path>
```

#### 3rdparty dependencies

Some 3rdparty dependencies might not be buildable on ceratin platforms,
they can therefore be switched off with specific defines.

##### Berkeleydb

Berkeley DB spooling is optional, when it is not available then the "classic" filebased spooling will be done.

```shell
cmake ... -DWITH_SPOOL_BERKELEYDB=OFF
```

##### PLPA

Currently PLPA is used for detecting a host's topology (it will be replaced soon by hwloc or boost compute).
It's use can be switched off, in this case no topology information is available within Gridengine.

```shell
cmake ... -DWITH_PLPA=OFF
```

##### Memory Allocators

By default we do not use the memory allocator coming with the OS but an optimized library.

On Linux this is the jemalloc memory allocator.

On Solaris it is the mtmalloc memory allocator.

In order to use the OS provided memory allocator these special allocators can be disabled by using
```shell
# on Linux
cmake ... -DWITH_JEMALLOC=OFF

# on Solaris
cmake ... -DWITH_MTMALLOC=OFF
```

#### Select Debug vs. Release mode

In Debug mode the code is built without optimization and with debug information;
additional checks might be activated in the code.

In a release build there is no debug information. Optimizations are turned on,
no potentially expensive special checks are done in the code.

By default the Debug build is active.

To switch to Relase build set the CMAKE_BUILD_TYPE variable:
```shell
cmake ... -DCMAKE_BUILD_TYPE=Release
```

With the Release build by default link time optimization is active (gcc option -flto).
It can be disabled with `-DENABLE_LTO=Off`.

### Build 3rdparty Dependencies

3rdparty dependencies (berkeleydb, jemalloc and plpa) need to be built once:

```shell
make 3rd_party
```

### Build Gridengine Code

```shell
make
```

To see the make targets set the VERBOSE variable:

```shell
make VERBOSE=1
```

Individual targets can be specified, e.g.

```shell
make sge_qmaster
```

### Install the Binaries and other Project Files

#### Configure what to install
When configuring the project installation targets can be set:
* INSTALL_SGE_BIN: Install all binaries and libraries including example binaries
* INSTALL_SGE_COMMON: Install common files (e.g. util, mpi templates, examples)
* INSTALL_SGE_DOC: Build and install documentation
* INSTALL_SGE_TEST: Install test binaries

When installing to a local directory you'll want to install all of them which is default.

When installing to a shared SGE_ROOT directory you'll want to
* install all on the build host for one of the target architectures
* build and install docs on one host being set up as doc build host
* install only binaries (and optionally test binaries) on build hosts for additional architectures

Select what to install when calling cmake, e.g.
```shell
cmake ... -DINSTALL_SGE_BIN=ON -DINSTALL_SGE_COMMON=OFF
```

#### Do the actual Installation

```shell
make install
```

## Sanitizers

Both gcc and clang allow the instrumentation of code with sanitizers.
cmake option `-DENABLE_SANITIZERS` enables instrumentation (only if it is a Debug build).
By default sanitizers are disabled.

Enabling sanitizers adds the following compiler and linker flags:
* `-fno-omit-frame-pointer`
* `-fsanitize=leak`
* `-fsanitize=undefined`
* `-fsanitize=address`

