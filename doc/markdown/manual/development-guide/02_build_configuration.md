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

## Define Source and Target Directories

Use the `-S` option to point it to the clusterscheduler repository:
```
cmake -S $OCS_BASE/clusterscheduler
```

If closed source extensions shall be built as well, then specify the path to their root directory
with *PROJECT_EXTENSIONS*. The *PROJECT_FEATURES* variable can then be set to "gcs-extensions" to overwrite
the default value of "clusterscheduler" to enforce that the product is built with enterprise features.

```
cmake ... -DPROJECT_EXTENSIONS=$OCS_BASE/oge-extensions -DPROJECT_FEATURES="oge-extension"
```

If you want to install to a different location than the default */opt/ge* specify *CMAKE_INSTALL_PREFIX*:

```
cmake ... -DCMAKE_INSTALL_PREFIX=$SGE_ROOT
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

[//]: # (Eeach file has to end with two emty lines)

