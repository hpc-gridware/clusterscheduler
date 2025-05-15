# Installing Munge

Munge is a lightweight authentication service for creating and validating credentials. It is used to secure communication between the xxQS_NAMExx components.

## Prerequisites for using Munge

* Munge must be installed on all hosts within the security realm (the cluster).
* All hosts within the security realm must have the same Munge key.
* The user and group set-up must be the same on all hosts (uid, gid, username, groupname).

## Creation of the Munge key on the first host

The munge key is usually located in `/etc/munge/munge.key`.

Depending on the OS Munge might already have been fully set up when the munge package was installed (e.g. on Ubuntu).

Use the `mungekey` command if it is available. It will create a Munge key and set up the correct permissions.
See the mungekey man page for more information, e.g. how to create a key with a specific key length.

```bash
/usr/sbin/mungekey
```

If the `mungekey` command is not available, you can create the key manually, e.g.:
```bash
dd if=/dev/urandom bs=1 count=1024 > /etc/munge/munge.key
chown munge:munge /etc/munge/munge.key
chmod 400 /etc/munge/munge.key
```

## Distributing the Munge key

The Munge key must be distributed to all hosts within the security realm (the cluster).   
Use your preferred way to distribute it safely.   
In the per OS instructions below, we will use a simple (and not necessarily secure) way to distribute the key: We copy it onto a shared file system (`/shared`) which we expect to be mounted on all hosts and will fetch it from there on all hosts.

## OS specific installation instructions

### Debian, Ubuntu, Raspbian

```bash
apt install -y munge
cp /shared/munge.key /etc/munge
chmod 400 /etc/munge/munge.key
chown munge:munge /etc/munge/munge.key
systemctl restart munge
systemctl status munge
```

For a compile host:
```bash
apt install libmunge-dev
```

### SUSE

```bash
zypper install -y munge
cp /shared/munge.key /etc/munge
chmod 400 /etc/munge/munge.key
chown munge:munge /etc/munge/munge.key
systemctl enable munge
systemctl start munge
systemctl status munge
```

For a compile host:
```bash
zypper install -y munge-devel
```

### CentOS, Rocky, Alma 8 and higher

```bash
dnf -y install munge
cp /shared/munge.key /etc/munge
chmod 400 /etc/munge/munge.key
chown munge:munge /etc/munge/munge.key
systemctl enable munge
systemctl start munge
systemctl status munge
```

For a compile host:
```bash
dnf -y install munge-devel
```

If this does not work:
```bash
dnf --enablerepo=powertools -y install munge-devel
```

### CentOS 7

```bash
yum -y install munge
cp /shared/munge.key /etc/munge
chmod 400 /etc/munge/munge.key
chown munge:munge /etc/munge/munge.key
systemctl enable munge
systemctl start munge
systemctl status munge
```

For a compile host:
```bash
yum -y install munge-devel
```

### CentOS 6

```bash
yum -y install munge
cp /shared/munge.key /etc/munge
chmod 400 /etc/munge/munge.key
chown munge:munge /etc/munge/munge.key
chkconfig munge on
/etc/init.d/munge start
/etc/init.d/munge status
```

For a compile host:
```bash
yum -y install munge-devel
```


### macOS 13/14

```bash
port install munge
cp /shared/munge.key /opt/local/etc/munge
chmod 400 /opt/local/etc/munge/munge.key
chown munge:munge /opt/local/etc/munge/munge.key
port load munge
```

### FreeBSD

* installs to /usr/local
* munge user is root
* the munge package contains munge daemon, lib and header files required for building xxQS_NAMExx

```bash
pkg install munge
cp /shared/munge.key /usr/local/etc/munge
chmod 400 /usr/local/etc/munge/munge.key
echo >> /etc/rc.conf
echo '# MUNGE' >> /etc/rc.conf
echo 'munged_enable="YES"' >> /etc/rc.conf
/usr/local/etc/rc.d/munged start
/usr/local/etc/rc.d/munged status
```

[//]: # (Eeach file has to end with two empty lines)

