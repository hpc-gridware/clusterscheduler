# Major Enhancements 

## v9.0.2beta

### Automatic Session Management

Patch 9.0.2 introduces the new concept of automatic sessions. This concept allows the xxQS_NAMExx system to synchronise internal data stores more efficiently, so that client commands can be enforced to get the most recent data. Session management is enabled, but can be disabled by setting the `DISABLE_AUTOMATIC_SESSIONS` parameter in the `qmaster_params` of the cluster configuration.

The default for the `qmaster_param` `DISABLE_SECONDARY_DS_READER` is now *false*. This means that the reader thread pool is enabled by default and does not need to be enabled manually as in patch 9.0.1.

What does all this mean? Sessions ensure that commands that trigger changes within the cluster, such as submitting a job, modifying a queue or changing a complex value, are executed in a consistent way. Sessions ensure that the result of changing commands in the cluster is immediately visible to the user who initiated the change. Commands that only read data, such as `qstat`, `qhost` or `qconf -s...`, always return the most recent data although all read-requests in the system are executed completely in parallel to the xxQS_NAMExx core components.

Unlike other workload management systems, session management in xxQS_NAMExx is automatic. There is no need to manually create or destroy sessions. Session management runs silently in the background to offload the most critical internal components.

All this further enhances cluster performance in large environments and improves cluster responsiveness, especially with tens of thousands of execution nodes, thousands of active users and millions of jobs/day. 

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)

## v9.0.1

### Utilization of additional data stores and activation of new thread pools

Starting with patch 9.0.1, the new internal architecture of `sge_qmaster` is enabled, allowing the component to use 
additional data stores that can be utilized by pools of threads.

* Listener threads: The listener thread pool was already available in earlier versions of Grid Engine. Starting with 
  version 9.0.0 of Cluster Scheduler, this pool received a dedicated datastore to forward incoming requests faster to 
  the component that ultimately has to process the request. New in version 9.0.1 is that this datastore includes more 
  information so that the listener threads themselves can directly answer certain requests without having to forward 
  them. This reduces internal friction and makes the cluster more responsive even in high load situations.

* Reader thread pool: The reader thread pool is activated and can now utilize a corresponding data store.
  This will boost the performance of clusters in large environments where also users tend to request the status of the
  system very often, by using client commands like `qstat`, `qhost` or other commands that send read-only requests 
  to `sge_qmaster`. The additional data store needs to be enabled manually by setting following parameter in the 
  *qmaster_params* of the cluster configuration:

  ```
  > qconf -mconf
  ...
  qmaster_params ...,DISABLE_SECONDARY_DS_READER=false
  ...
  ```
  
  Please note that requests answered by the reader thread pool might deliver slightly outdated data compared to the 
  requests answered with data from the main data store because both data stores can be slightly out of sync. The 
  maximum deviation can be configured by setting the `MAX_DS_DEVIATION` in milliseconds within in the `qmaster_params`.

  ```
  > qconf -mconf
  ...
  qmaster_params ...,MAX_DS_DEVIATION=1000 
  ...
  ```
  
  The default value is 1000 milliseconds. The value should be chosen carefully to balance the performance gain with
  the accuracy of the data.
 
  With one of the upcoming patches we will introduce an addition concept of automatic-sessions that will allow to
  synchronize the data stores more efficiently so that client commands can be enforced to get the most recent data.

* Enhanced monitoring: The monitoring of `sge_qmaster` has been enhanced to provide more detailed information about
  the utilization of the different thread pools. As also in the past the monitoring is enabled by setting the monitor 
  time:

  ```
  > qconf -mconf
  ...
  qmaster_params ...,MONITOR_TIME=10
  ...
  ```
  
  `qping` will then show statistics about the handled requests per thread.

  ``` 
  qping -i 1 -f  <master_host> $SGE_QMASTER_PORT qmaster 1
  ...
  10/11/2024 12:54:53 | reader: runs: 261.04r/s (
     GDI (a:0.00,g:2871.45,m:0.00,d:0.00,c:0.00,t:0.00,p:0.00)/s 
     OTHER (ql:0)) 
     out: 261.04m/s APT: 0.0007s/m idle: 80.88% wait: 0.01% time: 9.99s
  10/11/2024 12:54:53 | reader: runs: 279.50r/s (
     GDI (a:0.00,g:3074.50,m:0.00,d:0.00,c:0.00,t:0.00,p:0.00)/s 
     OTHER (ql:0)) 
     out: 279.50m/s APT: 0.0007s/m idle: 79.08% wait: 0.01% time: 10.00s
  10/11/2024 12:54:53 | listener: runs: 268.65r/s (
     in (g:268.34 a:0.00 e:0.00 r:0.30)/s 
     GDI (g:0.00,t:0.00,p:0.00)/s) 
     out: 0.00m/s APT: 0.0001s/m idle: 98.42% wait: 0.00% time: 9.99s
  10/11/2024 12:54:53 | listener: runs: 255.37r/s (
     in (g:255.37 a:0.00 e:0.00 r:0.00)/s 
     GDI (g:0.00,t:0.00,p:0.00)/s) 
     out: 0.00m/s APT: 0.0001s/m idle: 98.54% wait: 0.00% time: 10.00s
  ```
(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)

## v9.0.0

### qconf support to add/modify/delete/show complex entries individually

`Qconf` also allows you to add, modify, delete and display complexes individually using the new `-ace`, `-Ace`, 
`-mce`, `-Mce`, `-sce` and `-scel` switches. Previously this was only possible as a group command for the whole 
complex set with `-mq`. More information can be found in the qconf(1) man page or by running `qconf -help`.

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)

### Added support to supplementary group IDs in user, operator and manager lists. 

Additionally, to user and primary group names, it is now possible to specify supplementary group IDs in user, operator,
and manager lists. User lists can be specified in host, queue, configuration, and parallel environment objects to allow
or reject access. They are also used in resource quota sets or in combination with advance reservations.

The ability to specify supplementary groups allows administrators to set up a cluster so that access to users can be 
assigned based on their group membership. This is especially useful in environments where users are members of multiple 
groups and access to resources is based on group membership.

Furthermore, it is now possible to add group IDs to user lists if supplementary groups exist without a specific name. 
This can be helpful in environments where tools or applications are tagged with supplementary groups that are not 
managed via directory services like LDAP or NIS and therefore have no name.

Please note that the evaluation of supplementary groups is disabled by default. To enable it, set the parameter
`ENABLE_SUP_GRP_EVAL` to `1` in the `qmaster_params` of the cluster configuration. Enabling this feature can 
significantly impact the performance of directory services, especially in large clusters with many users and 
groups. Therefore, it is recommended to test the performance impact in a test environment before enabling it 
in a production environment. Enabling caching services like `nscd` can help reduce the performance impact.

(Available in Gridware Cluster Scheduler only)

### New internal architecture to support multiple Data Stores

The internal data architecture of `sge_qmaster` has been changed to support multiple data stores. This change 
does not have a major impact currently and is not visible to the user. However, it is a prerequisite for future 
enhancements that will significantly improve the scalability and performance of the cluster.
This will allow different processing components within `sge_qmaster` to use separate data stores, thereby 
enhancing the performance of the cluster in large environments.

(Available in Open Cluster Scheduler and Gridware Cluster Scheduler)

### New RSMAP (Resource Map) complex type

Resource Maps are a new complex type that allows administrators to define a list of special resources which
are available on a host, e.g. GPU devices, networking devices, lists of network ports, or other special resources.

Resource Maps are defined in the complex definition as other consumable resources, e.g.:

```
#name          shortcut   type       relop requestable consumable default  urgency 
#----------------------------------------------------------------------------------
gpu            gpu        RSMAP      <=    YES         HOST       NONE     0
```

The capacity of RSMAPs can be defined on execution host level and on global level (global host) in the `complex_values`
attribute.

The RSMAP capacity consists of two parts: The number of available resources and a list of specific resources.

Example for a GPU RSMAP:

```
$ qconf -se rocky-8-amd64-1
hostname              rocky-8-amd64-1
load_scaling          NONE
complex_values        gpu=4(GPU1 GPU2 GPU3 GPU4)
...
```

When a job is submitted the number of GPUs can be requested as resource request:  
`qsub -l gpu=2 myjob.sh`    

The job will be scheduled onto a host with at least 2 GPUs available.
Which GPUs have been assigned to a job can be retrieved by the `qstat -j` command:

```
$ qstat -j 4
==============================================================
job_number:                 4
...
resource_map           1:   gpu=rocky-8-amd64-1=(GPU1 GPU2)
```

The job script can access the assigned GPUs via the environment variable `SGE_HGR_<complex_name>`, in this example
`SGE_HGR_gpu`:
```
env | grep SGE_HGR
SGE_HGR_gpu=GPU1 GPU2
```

In addition to listing the available resources explicitly (`GPU1 GPU2 GPU3 GPU4`), it is also possible to define
ranges of resources.

Examples for port numbers on a host:
```
$ qconf -sce port_number
name        port_number
shortcut    port
type        RSMAP
relop       <=
requestable YES
consumable  HOST
default     NONE
urgency     0

$ qconf -se rocky-8-amd64-1 | grep complex_values
complex_values        port_number=100(65000-65099)

qrsh -l port_numbers=8 env | grep SGE_HGR
SGE_HGR_port_number=65000 65001 65002 65003 65004 65005 65006 65007
```

### Per HOST complex variables

The definition of complex variables contains the attribute `consumable` which could so far have the following values:

- `NO`: The complex is not consumable.
- `YES`: The complex is consumable and every task of a job consumes the requested amount of the resource.
- `JOB`: The complex is consumable and the requested amount of the resource is consumed once by the job (in case 
         of parallel jobs: by the master task).

With the new `HOST` consumable type the requested amount of the resource is consumed once per host. Even if in case
of parallel jobs multiple tasks are running on the same host, the requested amount of the resource is consumed only
once. E.g. multiple tasks of a parallel job can share the same GPU.


### One-line JSON format for accounting and reporting files

The accounting and reporting files contain one line per record.
The format of the records used to be a column-based format with a fixed number of columns,
column separator was the colon `:`.

The new acccounting and reporting file uses a one-line JSON format.
This makes it easier to structurise and extend the accounting and reporting records with new fields in the future,
and it makes it easier to parse the records with tools like `jq`, e.g.:

```
$ cat $SGE_ROOT/default/common/accounting.jsonl | jq  'select( .job_number == 4)'
{
  "job_number": 4,
  "task_number": 0,
  "start_time": 1725898198580794,
  "end_time": 1725898298874079,
  "owner": "joga",
  "group": "joga",
  "account": "sge",
  "qname": "all.q",
  "hostname": "rocky-8-amd64-1",
  "department": "defaultdepartment",
  "slots": 1,
  "job_name": "sleep",
  "priority": 0,
  "submission_time": 1725898190122999,
  "submit_cmd_line": "qsub -o /dev/null -j y -b y -l gpu=2 sleep 100",
  "category": "-l gpu=2",
  "failed": 0,
  "exit_status": 0,
  "usage": {
    "rusage": {
      "ru_wallclock": 100,
      "ru_utime": 0.082395,
      "ru_stime": 0.115703,
      "ru_maxrss": 17428,
      "ru_ixrss": 0,
      "ru_ismrss": 0,
      "ru_idrss": 0,
      "ru_isrss": 0,
      "ru_minflt": 9417,
      "ru_majflt": 76,
      "ru_nswap": 0,
      "ru_inblock": 28544,
      "ru_oublock": 8,
      "ru_msgsnd": 0,
      "ru_msgrcv": 0,
      "ru_nsignals": 0,
      "ru_nvcsw": 307,
      "ru_nivcsw": 0
    },
    "usage": {
      "wallclock": 100.90874,
      "cpu": 0.198098,
      "mem": 0.0020704269409179688,
      "io": 0.0010261209681630135,
      "iow": 0.0,
      "maxvmem": 222384128.0,
      "maxrss": 950272.0
    }
  }
}
```

In case the old format is required, e.g. as it is parsed by some custom script,
it can be enabled via the `reporting_params` configuration parameter.
in the global configuration.  
By default, the format is one-line JSON.  
By adding the parameter `old_accounting=true` the old format is used for accounting.  
By adding the parameter `old_reporting=true` the old format is used for reporting.

The old format is still supported for backward compatibility, but it is recommended to use the new default JSON format,
as extensions to the accounting and reporting records (e.g. more exact timestamps, the submission command line,
additional usage values like maxrss) are only done in the new format.


### Resource and queue requests per scope (global, master, slave) for parallel jobs

In former product versions resource requests for parallel jobs were only possible on the global level.
Resource requests for parallel jobs were applied to all tasks of the job, both master task (the job script)
and slave tasks. It was possible to define specific queues for the master task with the `-masterq` option.

Beginning with xxQS_NAMExx 9.0.0 it is possible to define resource request in 3 different scopes:
* global requests are applied to all tasks
* master requests are applied to the master task only
* slave requests are applied to the slave tasks only

The scope is selected by the `-scope` option of the submission command (qsub, qrsh, qlogin, qsh).  
Per scope it is possible to do resource requests and queue requests.

Example:

Submit a 16 times parallel job where the master task requires 1G memory and the slave tasks require 2G memory each
and a GPU.
The master task shall run in the `io.q` queue and the slave tasks in the `compute.q` queue.

```
qsub -pe mpi 16 -scope master -q io.q -l memory=1G \
     -scope slave -q compute.q -l memory=2G,gpu=1 job_script.sh
```

Using the -scope switch has the following constaints:

* It cannot be used with sequential jobs.
* Soft queue (-q) or resource requests (-l) are only allowed in the global scope.
* Resource requests (-l option) for a specific variable can be done either in the global scope,
  or in master and slave scope.
* Per host resource requests on a specific variable can only be done in one scope,
  either in global, in master or in slave scope.

The `-scope master -q qname` replaces the old `-masterq qname` option. The `-masterq` option is still supported
for backward compatibility, but it is recommended to use the new `-scope master` option.

Resource requests per scope can also be modified by `qalter` and via JSV scripts.

[//]: # (Eeach file has to end with two emty lines)

