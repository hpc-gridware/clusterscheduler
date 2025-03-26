# Major Enhancements

## v9.1.0

### Departments, Users and Jobs - Department View

The Department View, a feature first introduced in the Gridware Cluster Scheduler v9.0.2 patch, has been further enhanced and is now fully supported in Gridware Cluster Scheduler v9.1.0.

Cluster administrators can now define whether a user can see the Classic View, where all hosts, queues or jobs... are visible to everyone, or the Department View, where users can see only those cluster objects to which their department(s) have access.
Even if cluster administrators choose not to enforce the Department View, users can still use the functionality to reduce the number of visible objects, which can be useful in large clusters.

The following is a brief list of Department View features and the underlying changes to the user interface:

- Users are assigned to Departments. If a user is not assigned to at least one department, they are automatically assigned to the default department. Users can belong to more than one department, but this requires that such users specify one of the assigned departments when submitting jobs, so that for each job in the system it is known which department the jobs should be charged to (see `qsub -dept`).
- Departments/ACLs can be assigned to cluster objects such as the global configuration, hosts, queues, projects, ... or resource quota sets to allow or deny access to these objects. This restricts the access rights for user jobs (see *users_lists* in sge_conf(5), sge_host_conf(5), sge_queue_conf(5), sge_pe(5) or sge_resource_quota(5)) and the visibility of cluster objects in the user interface if the department view is enabled.
- The department view is enabled when a certain command line switch is used (`qhost/qstat/qselect, ... -sdv`).
- Various default files allow managers to force a user to view the department view. (`sge_qstat`, `sge_select`, ...). This will automatically hide all details about objects that do not belong to the department.

### Prevent Denial of Service Attacks

The Gridware Cluster Scheduler now includes a feature to prevent Denial of Service (DoS) attacks by limiting the number of requests accepted by the xxqs_name_sxx_qmaster(8) daemon per second. This limit is controlled by the `gdi_request_limits` parameter, which can be set in the global cluster configuration.

The syntax for this parameter is as follows:

```
gdi_request_limits := limit_rule [ ',' limit_rule ]* | 'NONE' .
limit_rule := source ':' type ':' object ':' user ':' host '=' limit .
```

The `source`, `type`, `object`, `user`, and `host` are filter criteria for the requests. Wildcard characters are supported, as well as user lists for `users` and host groups for `hosts`. The `limit` specifies the maximum number of requests accepted per second. More information on the syntax can be found in xxqs_name_sxx_conf(5).

Example:

```
gdi_request_limits=*:add:job:john:*=500,
                   *:add:job:*:*=50,
                   qstat:get:*:*:*=60000
```

In this example:
- The first rule allows user `john` to submit 500 jobs per second.
- The second rule allows all other users to submit 50 jobs per second.
- The third rule allows 60,000 `qstat` requests per second.
- All rules apply on all hosts independent where the client command is executed.

These rules are independent of the submit client used (e.g., `qsub`, `qrsh`, DRMAA client, or GUI). If a user exceeds the limit, the submit client will display an error message indicating the violated limit rule.

Note that one `qstat` command can trigger multiple GDI requests depending on the switches used. For example, `qstat -f` can query up to 15 different objects (job, queue, execution host, etc.) with one command. Therefore, the limit should be set high enough to allow users to get all necessary information in one command. For instance, a limit of 60,000 `get` requests allows about 5,000 `qstat -f` commands or 60,000 `qstat -j` commands per second.

(This feature is still in alpha state and may be subject to change till 9.1.0 FCS)

(Available in Gridware Cluster Scheduler only)

### Munge authentication

The Cluster Scheduler now supports Munge authentication. Munge is a lightweight authentication service that provides a secure way to authenticate users and services, see [https://dun.github.io/munge/](https://dun.github.io/munge/).

Munge is used to authenticate users in a xxQS_NAMExx cluster.  Munge authentication can be enabled at installation time or later by modifying the bootstrap file and re-starting all xxQS_NAMExx components.

See details in the Installation Guide, chapters "Planning the Installation" and "Installing Munge".

(Available in Gridware Cluster Scheduler and Open Cluster Scheduler)

### qsub -sync r

Additionally to the existing `qsub -sync y` option, new options have been introduced that allow to wait for certain job states.

`qsub -sync r` will wait for the job to be in the running state. In case of array jobs the command will wait for the start of the first task of the job.

(Available in Gridware Cluster Scheduler and Open Cluster Scheduler)

[//]: # (Eeach file has to end with two emty lines)

