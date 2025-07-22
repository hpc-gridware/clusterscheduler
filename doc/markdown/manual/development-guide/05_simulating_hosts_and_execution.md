# Simulating execution hosts and job execution

During development and for testing purposes it may be necessary to run huge clusters
and high numbers of jobs.

Having access to a huge cluster will often not be possible and also comes with high costs.
For many purposes no real hardware is required but hosts can be simulated.
There will be no communication between components on different hosts, but for many purposes this is not necessary.
For example for components like the scheduler thread it doesn't make a difference if a host really exists
or if it is just simulated.

## Simulating execution hosts and job execution

### Prerequisites

#### Hostname resolving

Hostnames of simulated hosts must be resolvable on the sge_qmaster machine (and on shadow hosts).

A script being part of the automated testsuite can be used to create host definitions which can be added
to /etc/hosts and/or cluster wide distribution mechanisms like NIS.

Usage:
```shell
testsuite/src/scripts/sim_eh.sh
usage: testsuite/src/scripts/sim_eh.sh <num_addresses>
```

Example output which can be appended to `/etc/hosts`
```shell
$ testsuite/src/scripts/sim_eh.sh 10
# ==============================================================================
# 10 simulated execution hosts for Cluster Scheduler
10.1.1.1 sim-eh-10-1-1-1
10.1.1.2 sim-eh-10-1-1-2
10.1.1.3 sim-eh-10-1-1-3
10.1.1.4 sim-eh-10-1-1-4
10.1.1.5 sim-eh-10-1-1-5
10.1.1.6 sim-eh-10-1-1-6
10.1.1.7 sim-eh-10-1-1-7
10.1.1.8 sim-eh-10-1-1-8
10.1.1.9 sim-eh-10-1-1-9
10.1.1.10 sim-eh-10-1-1-10
# ==============================================================================
```

#### Providing load information for simulated hosts

The simulated hosts shall have load values like `arch`, memory information, load information etc.

Therefore, the cluster must have at least one real host which will provide the load information.

Configuring which host is providing the load information is done with a complex variable `load_report_host`.

Add it by calling `qconf -mc` and adding the following line to the table of complex variables:

```shell
load_report_host    lrh        STRING      ==    YES         NO         NONE     0
```

#### Enabling the exechost simulation

Simulating exec hosts is enabled by setting a value in the global configuration, attribute `qmaster_params`.

Edit the global configuration with `qconf -mconf` and modify `qmaster_params` to
```shell
qmaster_params               SIMULATE_EXECDS=TRUE
```

Now simulated hosts can be added and job starts will be simulated in sge_qmaster.

### Adding simulated execution hosts

Add new hosts with `qconf -ae` or `qconf -Ae`.

Make sure to register a real host providing load information to the simulated hosts by setting `load_report_host` in the
simulated hosts `complex_values` attribute.

A simulated execution host can look like the following:

```shell
$ qconf -se sim-eh-10-1-1-87
hostname              sim-eh-10-1-1-87
load_scaling          NONE
complex_values        load_report_host=rocky-8-amd64-1
load_values           NONE
processors            0
user_lists            NONE
xuser_lists           NONE
projects              NONE
xprojects             NONE
usage_scaling         NONE
report_variables      NONE
```

## Simulating job starts on the execution hosts

By setting the execd_param SIMULATE_JOBS
job execution by sge_execd / via sge_shepherd can be prevented.
Instead, the job execution is just simulated.

@todo add details

[//]: # (Eeach file has to end with two empty lines)

