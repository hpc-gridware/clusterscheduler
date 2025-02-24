# Major Enhancements

## v9.1.0

### TODO: Major Topic

TODO

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

