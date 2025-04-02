# MPI examples

## testmpi

This directory holds an example for the MPI interface.
It is written in the C language and uses the MPI library to set up a parallel job
consisting of a master and a number of worker processes.

The master process sends commands to the worker processes, which execute the commands and return the results to the master.
The following commands exist:
* initialize: Initialize the worker process
* work: Execute one work package (which means spend about one second burning some cpu and sleep)
* usage: Get the usage of the worker process (cpu utime and stime, rss memory usage, via `getrusage()`)
* exit: Exit the worker process

In the end information about the processes is output to stdout and optionally written to a csv file.

The example supports checkpointing and restarting.

To build and run the example, create a working directory on a shared filesytem, e.g. on NFS and cd to it.

In the following we are using the MPICH implementation of MPI which we have installed to `~/3rd_party/mpich-4.3.0/lx-amd64`.

The instructions will work for other MPI implementations as well, just adjust the path to your installation.

### Commandline Options

```text
Usage: ./testmpi [-verbose] [-o csvfilename] [runtime in seconds]
  -verbose: verbose output
  -o csvfilename: write output to CSV file
  runtime in seconds, default is 60 seconds
```

## Build

```bash
export MPIR_HOME=~/3rd_party/mpich-4.3.0/lx-amd64
export PATH=$MPIR_HOME/bin:$PATH
mpicc -o testmpi $SGE_ROOT/mpi/examples/testmpi.c -lm
```

For Intel MPI:
```bash
mpicc -I$MPIR_HOME/include -L$MPIR_HOME/lib -o testmpi $SGE_ROOT/mpi/examples/testmpi.c -lm
```

You can run the resulting binary standlone without a scheduler, e.g.

```bash
$ mpirun -np 4 ./testmpi
Initializing MPI job with 4 ranks
Distributing 60 work units
      60 done
Retrieving usage
Shutting down MPI tasks
    Rank |       Processor |     work |    utime |    stime |   maxrss
---------+-----------------+----------+----------+----------+---------
       0 |          master |        0 |   20.102 |    0.006 |    10740
---------+-----------------+----------+----------+----------+---------
       1 | rocky-8-amd64-1 |       20 |    3.513 |    0.019 |    11044
---------+-----------------+----------+----------+----------+---------
       2 | rocky-8-amd64-1 |       20 |    3.518 |    0.018 |    10760
---------+-----------------+----------+----------+----------+---------
       3 | rocky-8-amd64-1 |       20 |    3.488 |    0.020 |    11072
---------+-----------------+----------+----------+----------+---------
```

## Configure the Cluster Scheduler

To run the example with the Cluster Scheduler, we need to create a parallel environment which we reference in a queue.   
Replace `mpich.pe` with the name/path of the PE template you want to use.

```bash
qconf -Ap $SGE_ROOT/mpi/mpich/mpich.pe
qconf -aattr queue pe_list mpich.pe all.q
```

If we want to use the checkpointing feature, we need to configure a checkpointing environment which also needs to be referenced in the queue.

```bash
qconf -Ackpt $SGE_ROOT/mpi/examples/testmpi.ckpt
qconf -aattr queue ckpt_list testmpi.ckpt all.q
```

## Run with Cluster Scheduler

The job script `testmpi.sh` is provided in this directory as well.

We can use `qsub` to submit the job or `qrsh` to run it in the foreground. Please note that checkpointing is only supported when running the job with `qsub`.

### Running interactively with qrsh

```bash
$ qrsh -verbose -pe mpich.pe 8 -l a=lx-amd64 -cwd -v MPIR_HOME $SGE_ROOT/mpi/examples/testmpi.sh
Your job 4 ("testmpi.sh") has been submitted
waiting for interactive job to be scheduled ...
Your interactive job 4 has been successfully scheduled.
Establishing builtin session to host ubuntu-22-amd64-1 ...
Initializing MPI job with 8 ranks
Distributing 60 work units
      60 done
Retrieving usage
Shutting down MPI tasks
    Rank |           Processor |     work |    utime |    stime |   maxrss
---------+---------------------+----------+----------+----------+---------
       0 |              master |        0 |    4.753 |    4.141 |     9720
---------+---------------------+----------+----------+----------+---------
       1 |   ubuntu-22-amd64-1 |        8 |    2.237 |    0.328 |    10140
---------+---------------------+----------+----------+----------+---------
       2 |   ubuntu-22-amd64-1 |        8 |    1.970 |    0.281 |     9960
---------+---------------------+----------+----------+----------+---------
       3 | 1.ubuntu-24-amd64-1 |        9 |    2.130 |    0.034 |    10752
---------+---------------------+----------+----------+----------+---------
       4 | 1.ubuntu-24-amd64-1 |        8 |    2.470 |    0.489 |    10368
---------+---------------------+----------+----------+----------+---------
       5 | 1.ubuntu-24-amd64-1 |        9 |    2.145 |    0.033 |    10496
---------+---------------------+----------+----------+----------+---------
       6 |   1.rocky-8-amd64-1 |        9 |    1.653 |    0.058 |    10428
---------+---------------------+----------+----------+----------+---------
       7 |   1.rocky-8-amd64-1 |        9 |    1.684 |    0.051 |    10360
---------+---------------------+----------+----------+----------+---------
```

## Running as batch job with qsub and with checkpointing

```bash
$ qsub -o testmpi.log -j y -pe mpich.pe 8 -l a=lx-amd64 -cwd -ckpt testmpi.ckpt -v MPIR_HOME $SGE_ROOT/mpi/examples/testmpi.sh -o testmpi.csv 600
Your job 7 ("testmpi.sh") has been submitted
```

We should see the job being submitted and running:

```bash
$ qstat
job-ID     prior   name       user         state submit/start at     queue                          slots ja-task-ID 
-----------------------------------------------------------------------------------------------------------------
         7 0.55500 testmpi.sh joga         r     2025-03-26 19:06:33 all.q@ubuntu-22-amd64-1            8
```

We can trigger checkpointing and restart of the job with qmod:

```bash
$ qmod -s 7
joga - suspended job 7
```

It will show up as suspended, the migration command of the checkpointing environment will be executed which will trigger checkpointing and exit of the job.
It will then be re-started.

```bash
$ qstat
job-ID     prior   name       user         state submit/start at     queue                          slots ja-task-ID 
-----------------------------------------------------------------------------------------------------------------
         7 0.55500 testmpi.sh joga         s     2025-03-26 19:06:33 all.q@ubuntu-22-amd64-1            8        
$ qstat
job-ID     prior   name       user         state submit/start at     queue                          slots ja-task-ID 
-----------------------------------------------------------------------------------------------------------------
         7 0.55500 testmpi.sh joga         Rq    2025-03-26 19:06:33                                    8        
$ qstat
job-ID     prior   name       user         state submit/start at     queue                          slots ja-task-ID 
-----------------------------------------------------------------------------------------------------------------
         7 0.55500 testmpi.sh joga         Rr    2025-03-26 19:06:53 all.q@ubuntu-22-amd64-1            8
```

The job output shows the two runs of the job:

```bash
$ cat testmpi.log 
Initializing MPI job with 8 ranks
Distributing 600 work units
Aborting work
      92 done
Retrieving usage
Shutting down MPI tasks
    Rank |           Processor |     work |    utime |    stime |   maxrss
---------+---------------------+----------+----------+----------+---------
       0 |              master |        0 |    7.487 |    6.300 |    10456
---------+---------------------+----------+----------+----------+---------
       1 |   ubuntu-22-amd64-1 |       13 |    3.107 |    0.307 |    10288
---------+---------------------+----------+----------+----------+---------
       2 |   ubuntu-22-amd64-1 |       13 |    3.205 |    0.318 |     9756
---------+---------------------+----------+----------+----------+---------
       3 | 1.ubuntu-24-amd64-1 |       13 |    3.562 |    0.435 |    10880
---------+---------------------+----------+----------+----------+---------
       4 | 1.ubuntu-24-amd64-1 |       13 |    3.557 |    0.485 |    10368
---------+---------------------+----------+----------+----------+---------
       5 | 1.ubuntu-24-amd64-1 |       14 |    3.250 |    0.062 |    10368
---------+---------------------+----------+----------+----------+---------
       6 |   1.rocky-8-amd64-1 |       13 |    2.853 |    0.649 |    10332
---------+---------------------+----------+----------+----------+---------
       7 |   1.rocky-8-amd64-1 |       13 |    2.838 |    0.643 |    12344
---------+---------------------+----------+----------+----------+---------
Initializing MPI job with 8 ranks
Restarting from checkpoint with 508 work units
Distributing 508 work units
     508 done
Retrieving usage
Shutting down MPI tasks
    Rank |         Processor |     work |    utime |    stime |   maxrss
---------+-------------------+----------+----------+----------+---------
       0 |            master |        0 |   43.122 |    0.390 |    17804
---------+-------------------+----------+----------+----------+---------
       1 | ubuntu-22-amd64-1 |       73 |   14.257 |    0.103 |    10448
---------+-------------------+----------+----------+----------+---------
       2 | ubuntu-22-amd64-1 |       72 |   14.393 |    0.086 |     9968
---------+-------------------+----------+----------+----------+---------
       3 | ubuntu-22-amd64-1 |       73 |   14.234 |    0.085 |    12532
---------+-------------------+----------+----------+----------+---------
       4 | ubuntu-22-amd64-1 |       72 |   14.605 |    0.057 |    12192
---------+-------------------+----------+----------+----------+---------
       5 | ubuntu-22-amd64-1 |       73 |   14.278 |    0.093 |    12676
---------+-------------------+----------+----------+----------+---------
       6 | ubuntu-22-amd64-1 |       73 |   14.249 |    0.055 |    12400
---------+-------------------+----------+----------+----------+---------
       7 | ubuntu-22-amd64-1 |       72 |   14.394 |    0.064 |    12260
---------+-------------------+----------+----------+----------+---------
```

We get the usage information in the csv file as well:

```bash
$ cat testmpi.csv
"Rank","Processor","work", "utime","stime","maxrss"
0,"master",0,7.487,6.300,10456
1,"ubuntu-22-amd64-1",13,3.107,0.307,10288
2,"ubuntu-22-amd64-1",13,3.205,0.318,9756
3,"1.ubuntu-24-amd64-1",13,3.562,0.435,10880
4,"1.ubuntu-24-amd64-1",13,3.557,0.485,10368
5,"1.ubuntu-24-amd64-1",14,3.250,0.062,10368
6,"1.rocky-8-amd64-1",13,2.853,0.649,10332
7,"1.rocky-8-amd64-1",13,2.838,0.643,12344
0,"master",0,43.122,0.390,17804
1,"ubuntu-22-amd64-1",73,14.257,0.103,10448
2,"ubuntu-22-amd64-1",72,14.393,0.086,9968
3,"ubuntu-22-amd64-1",73,14.234,0.085,12532
4,"ubuntu-22-amd64-1",72,14.605,0.057,12192
5,"ubuntu-22-amd64-1",73,14.278,0.093,12676
6,"ubuntu-22-amd64-1",73,14.249,0.055,12400
7,"ubuntu-22-amd64-1",72,14.394,0.064,12260
```