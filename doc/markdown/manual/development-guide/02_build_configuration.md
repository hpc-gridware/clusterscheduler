# Prepare the Build Configuration

`Cmake` requires a working directory where the built artifacts can be store. For performance reasons it is recommended
to have that directory on a non-shared filesystem. The recommended path is

*/usr/local/testsuite/$SGE\_MASTER\_PORT/build/$SGE\_ARCH* 

where \$SGE\_MASTER\_PORT is the sge\_qmaster port of the optional test system and where 
\$SGE\_ARCH is the architecture string of xxQS_NAMExx build
host (e.g */usr/local/testsuite/8004/build/lx-amd64*). Within this document this directory is referred
to as *\$OCS\_BUILD*.

```
cd $SGE_BUILD
cmake ... 
```

3rd-party components will be built in a separate directory to avoid the need for regular rebuilds.
You can overwrite the default of *\$HOME/3rdparty* with *PROJECT\_3RDPARTY\_HOME*
(e.g. /usr/local/testsuite/8004/build\_3rdparty). Within this document this directory is referred
to as *$OCS\_BUILD\_3RDPARTY*

```
cmake -DPROJECT_3RDPARTY_HOME=$OCS_BUILD_3RDPARTY
```

## Define Toolchain File

If you want to build on multiple different platforms, then you can use a toolchain file 
to define the compile environment for different target platforms/hosts. The toolchain file is a CMake script that defines 
the compiler, linker, and other tools to be used for the build process. You can specify the path to the toolchain 
file with the `-DCMAKE_TOOLCHAIN_FILE`. An example toolchain file is `cmake/Toolchain.cmake`. Either use this file 
or create your own toolchain file based on it and specify it with the following switch:

```
cmake -DCMAKE_TOOLCHAIN_FILE=$OCS_BASE/clusterscheduler/cmake/Toolchain.cmake
```

## Define Source and Target Directories

Use the `-S` option to point it to the clusterscheduler repository:
```
cmake -S $OCS_BASE/clusterscheduler
```

If closed source extensions shall be built as well, then specify the path to their root directory
with *PROJECT_EXTENSIONS*. The *PROJECT_FEATURES* variable can then be set to "gcs-extensions" to overwrite
the default value of "clusterscheduler" to enforce that the product is built with enterprise features.

```
cmake ... -DPROJECT_EXTENSIONS=$OCS_BASE/gcs-extensions -DPROJECT_FEATURES="gcs-extension"
```

If you want to install to a different location than the default */opt/ge* specify *CMAKE_INSTALL_PREFIX*:

```
cmake ... -DCMAKE_INSTALL_PREFIX=$SGE_ROOT
```

With *PROJECT_TESTSUITE* you can additionally point the build at a testsuite repository. The tests of that
testsuite are then registered as `ctest` tests, which makes them available in an IDE next to the C++ module
tests. This is optional and off by default. See *Register the Testsuite as CTest Tests* below.

```
cmake ... -DPROJECT_TESTSUITE=$OCS_BASE/testsuite -DTESTSUITE_LINE=92x
```

## Select Build Options

Some 3rd party dependencies may not be buildable on certain platforms, so they can be disabled with specific
definitions. Some dependencies may be optional for a platform that can be enabled with `cmake` defines.

1. **Berkeley DB** spooling is optional. It is enabled for most architectures but for some it is disabled.
   To deviate from the platforms default use the switch:
   ```
   cmake .. -DWITH_SPOOL_BERKELEYDB=OFF
   ```
   If BDB spooling is not available then classic (file based) spooling will be used.

2. Currently, *hwloc* is used for detecting a host's topology.
   It's use can be switched off, in this case no topology information is available within xxQS_NAMExx.

   ```
   cmake ... -DWITH_HWLOC=OFF
   ```

3. The *memory allocators* we do use by default is usually not that one coming with the OS but an optimized library.
   On Linux this is the *jemalloc* memory allocator. On Solaris it is the *mtmalloc* memory allocator.
   In order to use the OS provided memory allocator these special allocators can be disabled by using
   ```
   # on Linux
   cmake ... -DWITH_JEMALLOC=OFF

   # on Solaris
   cmake ... -DWITH_MTMALLOC=OFF
   ```
   
4. By default the required 3rdparty dependencies will be downloaded and installed by the cmake build.
   In order to use development packages provided by the OS use
   ```shell
   cmake ... -DWITH_OS_3RDPARTY=ON
   ```
   
5. Select *Debug* vs. *Release* build mode. In *Debug* mode the code is built without optimization and with
   debug information; additional checks might be activated in the code. In a *Release* build there is no debug
   information. Optimizations are turned on, no potentially expensive special checks are done in the code. By
   default, the *Debug* build is active. To switch to Release build set the *CMAKE_BUILD_TYPE* variable:
   ```
   cmake ... -DCMAKE_BUILD_TYPE=Release
   ```
   With the Release build by default link time optimization is active (`gcc` option `-flto`).
   It can be disabled with
   ```
   cmake ... -DENABLE_LTO=OFF
   ```
   Both `gcc` and `clang` allow the instrumentation of code with sanitizers. Following switch will enable
   instrumentation if the product is also build in *Debug* mode.
   ```
   cmake ... -DENABLE_SANITIZERS
   ```
   This enables following compiler and linker flags:  `-fno-omit-frame-pointer`, `-fsanitize=leak`,
   `-fsanitize=undefined` and `-fsanitize=address`

6. Specify the product parts that need to get installed as part of the `cmake` installation process. Following
   parts are available:

    * *INSTALL_SGE_BIN*  
   
      All binaries and libraries. Will overwrite *INSTALL_SGE_BIN_CLIENT*, *INSTALL_SGE_BIN_EXEC* and *INSTALL_SGE_BIN_MASTER*.
   
    * *INSTALL_SGE_BIN_CLIENT*
  
      All client binaries (e.g. `qconf`, `qstat`, `qsub`, etc.) and required libraries.
      Might be overwritten by the *INSTALL_SGE_BIN* option.
    
    * *INSTALL_SGE_BIN_EXEC*
   
      All server binaries required to run the execution daemon (e.g. `sge_execd`, `shepherd`, etc.) and required libraries
      Might be overwritten by the *INSTALL_SGE_BIN* option.

    * *INSTALL_SGE_BIN_MASTER*
   
      All components to run the master daemon (e.g. `sge_qmaster`, `sge_shadowd`, etc.) and required libraries
      Might be overwritten by the *INSTALL_SGE_BIN* option.
   
    * *INSTALL_SGE_COMMON* 
   
      Scripts and other common parts of the distribution that are not architecture specific
   
    * *INSTALL_SGE_DOC* 
   
      End user documentation (manuals and man pages)

    * *INSTALL_SGE_SRCDOC*

      Source code documentation generated by *Doxygen* (only available in *Debug* mode)
   
    * *INSTALL_SGE_TEST* 
   
      Test binaries required by the automated test environment
   

## Register the Testsuite as CTest Tests

The TCL/Expect based test environment can be made part of the CMake model. The tests of a release line are then
registered as `ctest` tests, so an IDE shows them in the same test tree as the C++ module tests and can run a
single test, a whole directory or a category from there. Nothing is added to the build itself: the branch
generates `add_test()` calls only, and it is completely inactive unless *PROJECT_TESTSUITE* is set.

* *PROJECT_TESTSUITE*

  Path to the testsuite repository (see above). Setting it includes *`<testsuite>/tools/CMakeLists.txt`*.
  Unset by default.

* *TESTSUITE_LINE*

  Release line whose testsuite is to be registered, e.g. *92x*. As long as it is empty nothing is registered
  even if *PROJECT_TESTSUITE* is set. The line also decides which test clusters the tests are run on.

* *TESTSUITE_RUNNERS*

  Number of parallel test clusters the tests are distributed over. Default is *8*. The clusters themselves are
  set up with `gcs-runners`; this value only has to match their number.

* *TESTSUITE_FILTER*

  Regular expression on the test name. Only matching tests are registered. Empty by default, which registers
  all of them.

  Restrict here rather than with `ctest -R` or `-L`: those filter at invocation time and `ctest` renumbers
  whatever survives, while an IDE re-running a single test from its results tree passes the index of the full
  list. Both together find nothing, or silently the wrong test. Switching test sets is therefore a CMake
  profile, not a run configuration.

```
cmake ... -DPROJECT_TESTSUITE=$OCS_BASE/testsuite \
          -DTESTSUITE_LINE=92x \
          -DTESTSUITE_RUNNERS=8
```

The test list is generated at configure time instead of being checked in, because a checkout adds and removes
tests; a stale list would not merely mislead but fail with *path not found in checktree*. The scan costs about
0.15 s and rides along with a CMake reload. If it fails (typically because the test clusters of the line are
not set up) this is only a *WARNING* and no tests are registered; a missing test setup never stops a product
build.

Setting up the test clusters, running the tests and the tools around it are described in
*`<testsuite>/tools/doc/`*.

## Check the Source Code Documentation (Optional)

The modules whose sources have been converted to Doxygen comments are listed as *DXM_FINISHED_MODULES* in
*`doc/doxygen/CMakeLists.txt`*. A separate Doxygen configuration, *`doc/doxygen/Doxyfile.strict`*, parses the
whole source tree so that cross module references resolve, but reports documentation defects for the listed
modules only. Modules that have not been converted are not listed and are therefore not checked.

This check is optional and is **not part of any build**. *DOXYGEN_STRICT* is *OFF* by default, which makes
`doc_doxygen_strict` an ordinary target rather than an `ALL` target. A fresh checkout, a regular build and the
CI workflows that build the documentation all remain unaffected by a missing or malformed comment: a
documentation defect is not a build defect.

* *DOXYGEN_STRICT*

  Adds the check to the `ALL` target of this build directory, so that every build fails on a documentation
  defect in one of the registered modules. *OFF* by default. This is meant for the build directory of whoever
  is converting a module, not as a default for everyone else.

Run the check explicitly, either from a build directory or without one:

```
# from a build directory; the target only exists with -DINSTALL_SGE_DOC=ON
cmake --build <build-dir> --target doc_doxygen_strict

# without a build directory; one or more paths relative to the clusterscheduler directory
doc/doxygen/dxm-check.sh source/libs/uti source/daemons/qmaster
```

The target is defined below *`doc/`* and therefore exists only when *INSTALL_SGE_DOC* is *ON*.
`dxm-check.sh` has no such dependency, runs in a few seconds and is the form to use while writing comments.
Both require `doxygen` in the *PATH*; if it is not installed, no Doxygen target is created at all.

This is unrelated to *INSTALL_SGE_SRCDOC*, which decides whether the generated HTML documentation becomes part
of the installation. The comment format, the conversion procedure and the rules for registering a module are
described in *`doc/doxygen/Readme-dxm.md`*.

## Trigger the Buildsystem Generator Via Command Line

When you have selected the required build configuration then you can run `cmake` in the build directory.
The next section shows an example for a Linux machine. The source code is located in a subdirectory of the
shared users home directory. 3rd-party and xxQS_NAMExx built artifacts are stored on the machines local disc under
*/usr/local/testsuite*. The product components will be installed directly into the $SGE_ROOT directory
which is also located in the shared home directory. The product will be build in *Debug* mode and all components
part of a regular distribution will be installed additionally to the test binaries except for the source code
documentation.

```
cd /usr/local/testsuite/8004/build/lx-amd64
cmake -S /home/ebablick/OCS/ge2/clusterscheduler \
      -DPROJECT_3RDPARTY_HOME=/usr/local/testsuite/8004/build_3rdparty \
      -DPROJECT_EXTENSIONS=/home/ebablick/OCS/ge2/oge-extensions \
      -DPROJECT_FEATURES="oge-extension" \
      -DCMAKE_INSTALL_PREFIX=/home/ebablick/OCS/ge2/inst \
      -DCMAKE_BUILD_TYPE=Debug \
      -DINSTALL_SGE_BIN=ON \
      -DINSTALL_SGE_COMMON=ON \
      -DINSTALL_SGE_DOC=ON \
      -DINSTALL_SGE_TEST=ON \
      -DINSTALL_SGE_SRCDOC=OFF
```

## Use an IDE as Build System

Here we use *CLion* as example because it provides full integration with CMake to build the source code.

1) Open the $OCS\_BASE directory as *CLion* project. This is the directory in which you cloned all
   xxQS_NAMExx related repositories   
2) Choose "CLion" => "Settings" to open the "Settings" Dialog
3) Goto section "Build, Execution, Deployment" => "CMake"
4) Set up a new profile (see the picture below). Set the build type. Omit the `-S` switch and the `-DMAKE_BUILD_TYPE` 
   in the *CMake options*. They are implicitly defined by the IDE. Accept your changes and close the dialog window.
 
   ![Clion's CMake Settings](__INPUT_DIR__/clion_settings_cmake.png)

5) In the *Project Browser* select the *CMakeLists.txt* file within the
   *clusterscheduler* folder, open the context menu and select "Load CMake Project". This step tells Clion the location of 
   the source code.
6) Add make options as needed (e.g. `-j` for a parallel build or *VERBOSE=1* to see the individual build commands
   during the build step)
7) Wait a moment. In the status line of the IDE you can find the background activities. CLion executes `cmake` and
   will load the project.

Next step is to build and install xxQS_NAMExx.

[//]: # (Each file has to end with two empty lines)


