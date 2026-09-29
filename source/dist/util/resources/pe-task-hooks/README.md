# Prolog and epilog for the tasks of parallel jobs

The queue prolog and epilog run only for the master task of a parallel job. The tasks a job starts
with `qrsh -inherit` (as MPI launchers do) run neither. `shepherd_wrapper.sh`, set as the
`shepherd_cmd`, adds them to the tasks of jobs which ask for it.

Details, safety rules and the side effect (the task gets a pty, which merges its stderr into its
stdout) are in the header of `shepherd_wrapper.sh`.

## Example

Verified on OCS 9.1.6 with a three-host tightly integrated PE (`make`).

**1. One hook script for master task and tasks**, set as queue prolog and epilog
(`qconf -mq all.q`: `prolog /opt/site/hook.sh prolog`, `epilog /opt/site/hook.sh epilog`).
Without a prefix the hooks run as the owner of the job; `<user>@/opt/site/hook.sh` runs them as
`<user>` instead, for example `root@` for a hook which needs root.

```sh
#!/bin/sh
# $1 is "prolog" or "epilog", as given in the queue configuration.
if [ -n "$SGE_PE_TASK_ID" ]; then
    echo "$1: task $SGE_PE_TASK_ID of job $JOB_ID on $(hostname) as $(id -un)"
else
    echo "$1: master task of job $JOB_ID on $(hostname) as $(id -un)"
fi
exit 0
```

**2. The wrapper**, with the tasks running the same prolog and epilog as the master task:

```sh
TASK_PROLOG=queue
TASK_EPILOG=queue
```

and set as the `shepherd_cmd` (`qconf -mconf`: `shepherd_cmd /opt/site/shepherd_wrapper.sh`).

**3. A job which starts one task per host**, the way an MPI launcher does:

```sh
#!/bin/sh
qrsh="$SGE_ROOT/bin/$($SGE_ROOT/util/arch)/qrsh"
for host in $(awk '{print $1}' "$PE_HOSTFILE"); do
    "$qrsh" -inherit -nostdin -V "$host" hostname
done
```

**4. Submitted with the hooks switched on:**

```
qsub -pe make 3 -v SGE_PER_TASK_PROLOG=1,SGE_PER_TASK_EPILOG=1 -j y -o with.out job.sh
```

`with.out` - every task runs the prolog and the epilog around its command, on its own host:

```
prolog: master task of job 41 on ocs-master as gridware
prolog: task 1.ocs-master of job 41 on ocs-master as gridware
ocs-master
epilog: task 1.ocs-master of job 41 on ocs-master as gridware
prolog: task 1.ocs-worker2 of job 41 on ocs-worker2 as gridware
ocs-worker2
epilog: task 1.ocs-worker2 of job 41 on ocs-worker2 as gridware
prolog: task 1.ocs-worker1 of job 41 on ocs-worker1 as gridware
ocs-worker1
epilog: task 1.ocs-worker1 of job 41 on ocs-worker1 as gridware
epilog: master task of job 41 on ocs-master as gridware
```

The same job without `-v SGE_PER_TASK_PROLOG=1,SGE_PER_TASK_EPILOG=1` - only the master task:

```
prolog: master task of job 42 on ocs-master as gridware
ocs-master
ocs-worker2
ocs-worker1
epilog: master task of job 42 on ocs-master as gridware
```

## Master task or task?

`SGE_PE_TASK_ID` is set in a task (for example `1.ocs-worker1`) and not in the master task. A job
can set it itself for its master task (`qsub -v SGE_PE_TASK_ID=...`), though. Where a wrong answer
matters, for example in a `root@` hook, ask the configuration execd wrote instead, which a job
cannot change:

```sh
if grep -q "^pe_task_id=" "$SGE_JOB_SPOOL_DIR/config"; then
```
