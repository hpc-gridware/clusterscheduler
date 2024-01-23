# Prepare a Host as Development Machine

In order to do development for the xxQS_NAMExx you need a host or virtual machine that provides following prerequisites.

## Software Prerequisites

For the xxQS_NAMExx core product the environment has to provide:

* cmake >= 3.0 
* git
* devtoolset-11 (gcc/g++ >= 11.2 or similar to support C++11) 
* autoconf
* automake
* patchelf

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
pkg install cmake git autoconf automake gettext bash
pkg install expect xterm gnuplot vim mailx
pkg install hs-pandoc texlive-full tex-xetex
```

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
apt-get install cmake git autoconf automake gcc g++ patchelf libntirpc-dev 
apt-get install expect xterm gnuplot tdom
```

### Rocky 8

```
dnf install cmake automake autoconf gcc-toolset-11 patchelf git libtirpc-devel
dnf install -y expect tcl gnuplot xterm libcgroup-tools perl-Env
dnf --enablerepo=devel install -y doxygen graphviz pandoc
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

