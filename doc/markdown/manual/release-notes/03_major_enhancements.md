# Major Enhancements

## v9.1.0

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
- The third rule allows 50,000 `qstat` requests per second.
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

