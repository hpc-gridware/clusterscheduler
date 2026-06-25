# Upgrade Notes

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

