# Prepare a Host as Development Machine

In order to do development for the xxQS_NAMExx you need a host or virtual machine that provides following prerequisites. Recommended build platform is CentOS Linux 8.x.

## Software Prerequisites

For the xxQS_NAMExx core product the environment has to provide:

* cmake >= 3.24 
* git
* devtoolset-11 (gcc/g++ >= 11.2 or similar to support C++17) 
* autoconf
* automake
* patchelf

Depending on the OS additional packages are required:

* libtirpc-devel (Linux)
* systemd-devel or libudev-dev (Linux), libudev-devd (FreeBSD)
* some cmake versions delivered with the OS are either too old or to new, and you need to install cmake yourself
  In such a case we recommend to install version 3.27.9.

Relying on os packages for the 3rdparty tools requires the following additional packages

* BDB
* hwloc 
* openssl

Documentation is written in markdown. To be able to generate product man pages and pdf manuals you need:

* pandoc
* texlive
* texlive-xetex

Source code documentation can be extracted via:

* Doxygen
* graphviz, ... (Dependencies depend also on the platform)

In order to test your changes you need to be able to run the automated testsuite which requires:

* TCL >= 8.5 
* Expect >= 5.45
* vim

Certain tests of the automated test environment require:

* xterm 
* mailx
* perl 
* python3
* ...

An IDE (e.g. Clion) is optional but might have addition prerequisites (e.g. rsync to support remote compilation). 

If you want to use *CLion* as development environment on Linux or macOS and remote compile for FreeBSD, then
you need to downgrade the `cmake` version to *3.27.9* on FreeBSD. The default `cmake` package provides *3.28.1*
which suites a manual build but is not supported by the *CLion IDE 2023.3.2*.

### CentOS 7 (default for ulx-amd64)

```
yum install -y centos-release-scl
yum install -y devtoolset-11 git autoconf automake flex bison patchelf
yum install -y numactl-devel ncurses-devel libXpm-devel pam-devel pciutils-devel systemd-devel
yum install -y epel-release
yum install -y tcsh expect
yum install -y libdb libdb-devel hwloc hwloc-devel openssl-devel
yum install -y java-1.8.0-openjdk java-1.8.0-openjdk-devel
```

### Debian 12

```
apt-get install -y binutils cmake autoconf automake libudev-dev
apt-get install -y build-essential manpages-dev git
apt-get install -y doxygen graphviz
apt-get install -y expect tcl tdom gnuplot vim
apt-get install -y tcsh xterm expect tcl gnuplot
```

### FreeBSD 13/14

```
pkg install cmake git autoconf automake gettext bash libudev-devd
pkg install expect xterm gnuplot vim mailx
pkg install hs-pandoc texlive-full tex-xetex
pkg install openjdk8-8.402.06.1_1
```

### macOS 14 (default for darwin-arm64)

```
XCodeUpdate 01_prepare_dev_env.md
XQuartz (for some testsuite tests)
brew install cmake git autoconf automake gettext
brew install expect tcl-tk gnuplot perl
brew install pandoc texlive
brew install doxygen graphviz
```

### Raspian 11

```
apt-get install git autoconf automake gcc g++ patchelf libntirpc-dev libudev-dev
apt-get install expect xterm gnuplot tdom
```

`cmake` needs to be (compiled and) installed manually because the default `cmake` package just provides version 3.18.

### Rocky 8 / Alma 8 / CentOS 8 (default for lx-amd64, lx-arm64, lx-ppc64le)
and only for lx-amd64

```
dnf install -y automake autoconf cmake patchelf git libtirpc-devel systemd-devel patchelf
dnf install -y gcc-toolset-11 gcc-toolset-11-libasan-devel gcc-toolset-11-libubsan-devel
dnf install -y expect tcl gnuplot xterm libcgroup-tools perl-Env tcsh
dnf config-manager --set-enabled powertools
dnf --enablerepo=devel install -y doxygen graphviz pandoc
dnf install -y texlive*
dnf install -y java-1.8.0-openjdk java-1.8.0-openjdk-devel
```

Depending on the OS patchlevel `cmake` needs to be (compiled and) installed manually because the default `cmake` package might just provide versions < 3.24

### Solaris 11 (default for sol-amd64)

```
pkg install pkg:/developer/build/automake
pkg install pkg:/developer/build/autoconf 
pkg install pkg:/developer/gcc-11
pkg install pkg:/developer/versioning/git
pkg install pkg:/developer/debug/gdb
pkg install pkg:/terminal/xterm
pkg install pkg:/library/security/openssl
pkg install pkg:/shell/expect
pkg install pkg:/developer/documentation-tool/doxygen
pkg install pkg:/image/graphviz
pkg install pkg:/image/gnuplot
pkg install pkg:/developer/java/jdk-8
```

### SUSE Tumbleweed (default for lx-riscv64)

```
zypper install --type pattern devel_C_C++
zypper install gdb cmake git patchelf 
zypper install openssl libtirpc-devel systemd-devel
zypper install doxygen graphviz
zypper install tcl expect vim xterm mailx perl gnuplot
zypper install texlive pandoc
```

### Ubuntu 20/22/24

```
apt-get install -y git autoconf automake build-essential manpages-dev libudev-dev
apt-get install -y expect tcl tdom gnuplot xterm
apt-get install -y doxygen graphviz pandoc 
apt-get install -y rapidjson-dev libdb5.3 libdb5.3-dev
apt-get install -y libjemalloc2 libjemalloc-dev hwloc libhwloc-dev
apt-get install -y pandoc texlive*
```

## Clone the Required Repositories

Create a directory that will hold all git repositories for xxQS_NAMExx. Within this document this directory is referred 
to as *$OCS_BASE*. 

```
cd $OCS_BASE
git clone https://github.com/hpc-gridware/clusterscheduler
git clone https://github.com/hpc-gridware/testsuite
```

If you want to build with closed source extensions then additionally clone

```
git clone https://github.com/hpc-gridware/gcs-extensions
git clone https://github.com/hpc-gridware/gcs-testsuite
```

