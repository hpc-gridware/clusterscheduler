# Building with cmake

## Prerequisits

* gcc >= 
* g++
* cmake >= 3.0
* autoconf
* git

## Building Gridengine


### Configure the Project

Create a local build directory and cd into it.

```shell
cmake -S path_to_gridengine_directory
```

If you want to install to a different location than the default (`/opt/ge`) specify
`CMAKE_INSTALL_PREFIX`:

```shell
cmake -S <path_to_gridengine_directory> -DCMAKE_INSTALL_PREFIX=<install_path>
```

### Build 3rdparty Dependencies

3rdparty dependencies (berkeleydb, jemalloc and plpa) need to be built once:

```shell
make 3rd_party_berkeleydb 3rd_party_jemalloc 3rd_party_plpa
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

```shell
make install
```


