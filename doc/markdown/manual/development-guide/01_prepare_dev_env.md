# Prepare a Host as Development Machine

In order to do development for the xxQS_NAMExx you need a host or virtual machine that provides following prerequisites.

## Software Prerequisites

For the xxQS_NAMExx core product the environment has to provide:

* cmake >= 3.24 (if you need to install cmake yourself because the OS package does not fulfill the requirements then 
  install version 3.27.9)
* git
* devtoolset-11 (gcc/g++ >= 11.2 or similar to support C++17) 
* autoconf
* automake
* patchelf

Depending on the OS additional packages are required:

* libtirpc-devel (Linux)
* systemd-devel (Linux)

Documentation is written in markdown. To be able to generate product man pages and pdf manual you need:

* pandoc
* texlive
* texlive-xetex

Source code documentation can be extracted via:

* Doxygen

In order to test your changes you need to be able to run the automated testsuite which requires:

* TCL >= 8.5 
* Expect >= 5.45
* vim

Certain tests of the automated test environment require:

* xterm 
* mailx
* perl 
* python3

An IDE is optional but might have addition prerequisites (e.g. rsync to support remote compilation).

### FreeBSD 13/14

```
pkg install cmake git autoconf automake gettext bash libudev-devd
pkg install expect xterm gnuplot vim mailx
pkg install hs-pandoc texlive-full tex-xetex
```

If you want to use *CLion* as development environment on Linux or macOS and remote compile of FreeBSD, then 
you need to downgrade the `cmake` version to *3.27.9*. The default `cmake` package provides *3.28.1* which suites 
a manual build but is not supported by the *CLion IDE 2023.3.2*.

### macOS

```
XCode XQuartz
brew install cmake git autoconf automake gettext
brew install expect tcl-tk gnuplot perl
brew install pandoc texlive
brew install doxygen graphviz
```

### Raspian

```
apt-get install git autoconf automake gcc g++ patchelf libntirpc-dev libudev-dev
apt-get install expect xterm gnuplot tdom
```

`cmake` needs to be (compiled and) installed manually because the default `cmake` package just provides version 3.18.

### Rocky 8 / Alma 8 / CentOS 8

```
dnf install -y automake autoconf gcc-toolset-11 patchelf git libtirpc-devel systemd-devel
dnf install -y expect tcl gnuplot xterm libcgroup-tools perl-Env
dnf --enablerepo=devel install -y doxygen graphviz pandoc
```

`cmake` needs to be (compiled and) installed manually because the default `cmake` package just provides version 3.20. 

### Ubuntu 22
```
apt-get install -y ... libudev-dev
```
    
## Clone the Required Repositories

Create a directory that will hold all git repositories for xxQS_NAMExx. Within this document this directory is referred 
to as *$OGE_BASE*. 

```
cd $OGE_BASE
git clone https://github.com/opengridengine/gridengine
git clone https://github.com/opengridengine/testsuite
```

If you want to build with closed source extensions then additionally clone

```
git clone https://github.com/opengridengine/oge-extensions
git clone https://github.com/opengridengine/oge-testsuite
```

