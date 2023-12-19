# Building with cmake

## Prerequisits

* gcc >= 
* g++
* cmake >= 3.0
* autoconf, automake
* git

## Building Gridengine


### Configure the Project

#### Pointing cmake to the source directory
Create a local build directory and cd into it.

Use the cmake -S option to point it to the gridengine repository (the toplevel directory)

```shell
cmake -S path_to_gridengine_directory
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
* INSTALL_SGE_DOC: Build and install documentation (TBD)
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


