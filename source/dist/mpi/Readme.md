# Cluster Scheduler Parallel Job Support for MPI

This directory contains templates of parallel environments (PE) and scripts
for the integration of Cluster Scheduler with various MPI implementations,
as well as an example MPI job which can be used for testing purposes.

## Note on the Cluster Scheduler Parallel Job Integration Modes

Parallel jobs can be run in Cluster Scheduler with loose integration or with tight integration.

Loose integration means that the master task (the job script) of the parallel job is started
by Cluster Scheduler, but that the
slave tasks are not controlled by Cluster Scheduler. This means that the
parallel tasks are not accounted, and that the resource limits are not
enforced for the parallel tasks.

It is in the responsibility of the application to start the tasks based on a generated host file
and to make sure that they are terminated correctly.
Usually the start of slave tasks on remote hosts is done by using `ssh`.

With tight integration, the parallel tasks are started under Cluster Scheduler
controll. This means that the parallel tasks
are accounted, and that the resource limits are enforced for the parallel tasks.
Cluster Scheduler also makes sure that the parallel tasks are terminated correctly.

The parallel tasks must be started by using the `qrsh -inherit <hostname> <cmd> <args>` command.

Most MPI implementations support tight integration with Cluster Scheduler.
It uses the same interface as the former Sun Grid Engine (SGE) implementation.

For cases where an MPI implementation (or another parallel application) does not
support SGE tight integration, Cluster Scheduler provides a template for a parallel
environment (PE) which installs a wrapper for the
`ssh` command, which in turn calls `qrsh -inherit` instead of `ssh`.

## Examples

In the `examples` directory you will find an example of a MPI job (written in the C language)
as well as a job script and a template for a checkpointing environment.
The MPI test application comes with its own Readme.md file, which describes how to build and run it.

## intel-mpi

The [Intel MPI Library](https://www.intel.com/content/www/us/en/developer/tools/oneapi/mpi-library.html) is
Intel's implementation of the Message Passing Interface (MPI) standard. It is
a high-performance, low-latency implementation of MPI that is optimized for
Intel architectures. The Intel MPI Library is part of the Intel oneAPI
toolkit, which provides a comprehensive set of tools and libraries for
developing high-performance applications.

Install the parallel environment for Intel MPI and add it to a queue with the following commands:

```bash
qconf -Ap $SGE_ROOT/mpi/intel-mpi.pe
qconf -aattr queue pe_list intel-mpi.pe <queue_name>
```

## mpich
The [MPICH](https://www.mpich.org/) implementation of the Message Passing Interface (MPI) standard is a high-performance,
open-source implementation of the MPI standard. It is designed to be portable and efficient, and it is widely used in high-performance computing (HPC) environments.
MPICH is the reference implementation of the MPI standard and serves as a foundation for many other MPI implementations.
MPICH is designed to be highly portable and can run on a wide range of platforms, including clusters, supercomputers, and cloud environments. It supports various network interconnects, including Ethernet, InfiniBand, and shared memory.

The `mpich` directory contains a PE template and a build script which can be used to build and install arbitrary versions of MPICH. Modify the build script, especially the options passed to `configure`, to suit your needs. The build script will download the source code, build it, and install it in the specified directory.

To install the parallel environment for MPICH and add it to a queue, use the following commands:

```bash
qconf -Ap $SGE_ROOT/mpi/mpich/mpich.pe
qconf -aattr queue pe_list mpich.pe <queue_name>
```

## mvapich

The [MVAPICH](https://mvapich.cse.ohio-state.edu/) implementation of the Message Passing Interface (MPI) standard is a high-performance,
open-source implementation of the MPI standard. It is designed to be portable and efficient, and it is widely used in high-performance computing (HPC) environments.
MVAPICH is based on the MPICH implementation and is optimized for high-performance networks, such as InfiniBand and Omni-Path. It provides support for both shared memory and distributed memory programming models, making it suitable for a wide range of applications.

The `mvapich` directory contains a PE template and a build script which can be used to build and install arbitrary versions of MVAPICH. Modify the build script, especially the options passed to `configure`, to suit your needs. The build script will download the source code, build it, and install it in the specified directory.

To install the parallel environment for MVAPICH and add it to a queue, use the following commands:

```bash
qconf -Ap $SGE_ROOT/mpi/mvapich/mvapich.pe
qconf -aattr queue pe_list mvapich.pe <queue_name>
```

## openmpi

The [Open MPI](https://www.open-mpi.org/) implementation of the Message Passing Interface (MPI) standard is a high-performance,
open-source implementation of the MPI standard. It is designed to be portable and efficient, and it is widely used in high-performance computing (HPC) environments.
Open MPI is a collaborative project that combines the efforts of several research and academic institutions, as well as commercial vendors. It is designed to be modular and extensible, allowing users to customize the implementation for their specific needs. Open MPI supports a wide range of network interconnects, including Ethernet, InfiniBand, and shared memory, making it suitable for various computing environments.

The `openmpi` directory contains a PE template and a build script which can be used to build and install arbitrary versions of Open MPI. Modify the build script, especially the options passed to `configure`, to suit your needs. The build script will download the source code, build it, and install it in the specified directory.

To install the parallel environment for Open MPI and add it to a queue, use the following commands:

```bash
qconf -Ap $SGE_ROOT/mpi/openmpi/openmpi.pe
qconf -aattr queue pe_list openmpi.pe <queue_name>
```


## ssh-wrapper

The `ssh-wrapper` is a script that replaces the `ssh` command with a wrapper that uses the `qrsh -inherit` command instead. This allows for tight integration of parallel jobs with Cluster Scheduler, ensuring that the parallel tasks are accounted and resource limits are enforced.

Use it for applications which are not integrated with Cluster Scheduler and which start tasks on remote hosts using `ssh`.

To install the parallel environment for the `ssh-wrapper` and add it to a queue, use the following commands:

```bash  
qconf -Ap $SGE_ROOT/mpi/ssh-wrapper/ssh-wrapper.pe
qconf -aattr queue pe_list ssh-wrapper.pe <queue_name>
```


