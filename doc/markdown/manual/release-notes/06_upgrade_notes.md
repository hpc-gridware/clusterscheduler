# Upgrade Notes

## v9.1.5

### TLS: Private Keys Move Below the Cell

The private keys of the daemons used to be stored in `/var/lib/ocs/<qmaster_port>/private`. The path now also
contains the cell: `/var/lib/ocs/<qmaster_port>/<cell>/private`. The port alone did not identify an
installation, so two clusters using the same port - a side-by-side upgrade after the switch over, or a
reinstallation into another cell - shared the directory, and the one that did not write the key failed to start
with *key values mismatch*
([CS-2487](https://hpc-gridware.atlassian.net/browse/CS-2487)).

Nothing has to be done for the upgrade: the daemons notice that the key of their certificate is not in the new
place, and create certificate and key anew on the first start. The old files stay behind and are no longer read.
They can be removed once every daemon on the host has been started with the new version:

    rm -f /var/lib/ocs/<qmaster_port>/*.pem /var/lib/ocs/<qmaster_port>/private/*.pem

Removing them is not done automatically - xxQS_NAMExx does not delete key material it may not have created
itself.

### Systemd Unit Files: File Descriptor Limit

A systemd service does not inherit the settings of `/etc/security/limits.conf`. A unit file without a
`LimitNOFILE=` setting therefore runs with systemd's default soft limit of 1024 file descriptors.

That limit matters for `sge_qmaster`: it derives the maximum number of dynamic event clients - used by
`qsub -sync y` and by DRMAA sessions - from its soft file descriptor limit, and with 1024 descriptors the
maximum is silently capped at 979, below the default of 1000. See `MAX_DYN_EC` in xxqs_name_sxx_conf(5).

The unit file template of the qmaster service now sets:

```text
LimitNOFILE=65536
```

The template of the execd service carries the same setting commented out. It is not enabled by default
because the limit is inherited by the jobs: `sge_shepherd` sets a job's file descriptor limit only when the
execd parameters `S_DESCRIPTORS` / `H_DESCRIPTORS` are configured, so enabling it would raise the limit for
every job on the host.

*An upgrade does not touch the unit files of an existing installation.* They were generated at installation
time and are left alone, so an upgraded cluster keeps the old limit until the unit files are updated. There
are two ways to do that.

As user `root`, edit the unit file `/etc/systemd/system/ocs<qmaster_port>-qmaster.service`, add the
`LimitNOFILE` setting to the `[Service]` section, and reload and restart the service:

```bash
systemctl daemon-reload
systemctl restart ocs<qmaster_port>-qmaster
```

Alternatively let the installer recreate the unit files from the current templates:

```bash
cd $SGE_ROOT
./inst_sge -upd-rc
```

This removes the existing unit files and installs new ones on the qmaster, all shadow hosts and all
execution hosts. Note that it *stops the daemons* whose unit file it replaces and does not start them again -
the new unit is installed and enabled, but the cluster has to be started afterwards, for example with
`inst_sge -start-all`. Run it during a maintenance window, and note that it needs `root` and reaches the
other hosts over ssh; use `-noremote` to work on the local host only.

## v9.1.3

### Systemd unit file for the ocs-qmaster.service

With the fix for  
*[CS-2357](https://hpc-gridware.atlassian.net/browse/CS-2357) On shadow-only hosts the qmaster systemd service (ocs<port>-qmaster.service) starts sge_shadowd successfully but then immediately stops it again*  
a new setting was added to the ocs-qmaster.service unit file.

To update the unit file for an existing GCS 9.1.x installation,
as user `root` edit the unit file  
`/etc/systemd/system/ocs<qmaster_port>-qmaster.service`  
and add the `GuessMainPID=no` setting, e.g.,

```text
[Unit]
Description=Cluster Scheduler sge_qmaster and optionally sge_shadowd services
Documentation=man:sge_qmaster(8) man:sge_shadowd(8)
After=network-online.target remote-fs.target autofs.service

[Service]
Type=forking
GuessMainPID=no
Slice=ocs8028.slice
...
```

Update systemd with
```bash
systemctl daemon-reload
```


[//]: # (Each file has to end with two empty lines)

