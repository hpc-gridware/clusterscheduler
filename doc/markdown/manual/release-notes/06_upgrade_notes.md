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

