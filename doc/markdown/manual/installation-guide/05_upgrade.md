# Updates and Upgrades

This chapter describes how xxQS_NAMExx is kept up to date: which kinds of releases exist, how
security updates are provided, and how a patch release or a new minor or major version is installed
with as little impact on the running workload as possible.

## Release Types

The version string of xxQS_NAMExx has three parts: the major version, the minor version, and the
patch level. In version 9.1.7, 9 is the major version, 1 is the minor version, and 7 is the patch
level.

* **Patch release** (e.g. 9.1.6 to 9.1.7). A patch release contains bug fixes, including fixes for
  security vulnerabilities. It does not require changes to the configuration or to the spooled data,
  and it is installed by replacing the binaries, see [Patch Installation](#patch-installation).
  Running jobs keep running. Security fixes are always delivered in patch releases, separately from
  new functionality.

* **Minor or major release** (e.g. 9.0.x to 9.1.x). A minor or major release contains new
  functionality and may contain incompatible changes that require changes to configuration files or
  to the database schema. It is installed with an upgrade procedure, either side-by-side with the
  existing cluster, see [Side-by-Side Upgrade](#side-by-side-upgrade), or in place, see [In-Place
  Upgrade](#in-place-upgrade). Alternatively, the software can be reinstalled.

There are exceptions to these rules. Before installing any release, read its release notes, in
particular the chapter *Upgrade Notes*, which lists the additional steps a release requires, and
back up the cluster, see chapter *Backup and Restore*.

xxQS_NAMExx cannot be upgraded from other Grid Engine products. Their installation steps are almost
identical to those of xxQS_NAMExx, and a new installation is strongly recommended, even if a
side-by-side upgrade is at least partially possible.

If you have questions, contact the xxQS_COMPANY_NAMExx support team or submit a question through
the support portal at <https://hpc-gridware.atlassian.net/servicedesk/customer/portal/1>.

## Security Updates

[//]: # (TODO CS-1667: confirm support period, backporting, advisories - CS-1658, CS-1660)

### Support Period

xxQS_COMPANY_NAMExx publishes the support period of each release line (e.g. 9.1.x) on its web site,
including the month and the year in which it ends. During the support period of a release line,
vulnerabilities are fixed and the fixes are published as patch releases, free of charge. A
vulnerability may be fixed in the latest release line only. The fix is then available by upgrading
to the latest release line.

Every security update stays available for download for at least 10 years after its release, or until
the end of the support period, whichever is later.

### Notification About Security Updates

The release notes of every release list the vulnerabilities it fixes. For a fixed vulnerability,
xxQS_COMPANY_NAMExx publishes a security advisory, which describes the vulnerability, the affected
versions, its severity, and the steps an administrator has to take.

To report a vulnerability, use the web form at
[https://www.hpc-gridware.com/security](https://www.hpc-gridware.com/security). The file
`SECURITY.md` in the source repository describes how reports are handled.

### No Automatic Updates

xxQS_NAMExx does not install updates automatically. An update of a cluster has to be coordinated
across all hosts of the cluster, and its timing depends on the running workload, so the
administrator decides when an update is installed. Install security updates as soon as possible
after they are published. A patch release can be installed while jobs keep running, see [Patch
Installation](#patch-installation).

### Third-Party Components

xxQS_NAMExx uses two kinds of third-party components:

* **Components shipped with xxQS_NAMExx**, e.g. Berkeley DB, hwloc, and jemalloc. They are part of
  the xxQS_NAMExx packages, and their security fixes are delivered with xxQS_NAMExx releases.

* **Libraries of the operating system**, e.g. the C and C++ runtime libraries, OpenSSL, MUNGE,
  systemd, and the PostgreSQL client library libpq, as well as the Java runtime used by DBWriter.
  They are updated with the package manager of the operating system. Keep them up to date. A daemon
  loads these libraries at startup, so restart the xxQS_NAMExx daemons after updating them, see step
  7 of [Patch Installation](#patch-installation).

The software bill of materials (SBOM) lists all components and the versions of the components
shipped with xxQS_NAMExx. It is installed as `$SGE_ROOT/3rd_party/sbom.cyclonedx.json` in the
CycloneDX format, which vulnerability scanners can read. Not every vulnerability reported for a
component affects xxQS_NAMExx. `$SGE_ROOT/3rd_party/vex.cyclonedx.json` lists the vulnerabilities
that do not, with the reason, in the CycloneDX VEX format. Vulnerability scanners like
Dependency-Track import it and record the reasons with their findings. The licenses of the
third-party components are listed in `$SGE_ROOT/3rd_party/3rd_party_licscopyrights.md`.

## Patch Installation

A patch release is installed by replacing the binaries and libraries of the installation. The steps
below keep jobs running during the installation. The daemons are unavailable only while they are
restarted.

1. Back up your cluster, see chapter *Backup and Restore*.

2. Download the packages of the patch release from xxQS_COMPANY_NAMExx, verify their checksums as
   described in chapter *Download*, and read the release notes. If the release notes do not mention
   additional steps, continue with these instructions.

   Jobs and daemons can keep running during steps 3 to 6. Until step 7 has been completed, however,
   no commands can be run and no daemons can be started.

3. If `$SGE_ROOT` is not on a shared filesystem, perform steps 4 to 8 on every host of the cluster.

4. As user root, move, but do *not* remove, the directories containing the binaries and libraries.
   The directories have to remain on the same filesystem.

   ```
   # cd $SGE_ROOT
   # mv bin bin.old
   # mv lib lib.old
   # mv utilbin utilbin.old
   ```

   Running processes, e.g. the `sge_shepherd` of a running job, keep using the old binaries and
   libraries.

5. Unpack the new packages in the `$SGE_ROOT` directory.

   ```
   # tar xfz xxqs_name_sxx-<version>-*.tar.gz
   ```

6. Set the ownership and the file permissions of the new files.

   ```
   # util/setfileperm.sh $SGE_ROOT
   ```

7. Restart the daemons: `sge_qmaster` on the master host, `sge_shadowd` on every shadow host, and
   `sge_execd` on every execution host.

   If the daemons run as systemd services, restart the services. `<slice_name>` is the name stored
   in `$SGE_ROOT/$SGE_CELL/common/slice_name`. The service `<slice_name>-qmaster` runs `sge_qmaster`
   and, on a shadow host, `sge_shadowd`.

   ```
   # systemctl restart <slice_name>-qmaster    # on the master host and on every shadow host
   # systemctl restart <slice_name>-execd      # on every execution host
   ```

   Otherwise, use the startup scripts. `softstop` stops `sge_execd` without stopping its jobs.

   ```
   # $SGE_ROOT/$SGE_CELL/common/sgemaster stop             # on the master host
   # $SGE_ROOT/$SGE_CELL/common/sgemaster start
   # $SGE_ROOT/$SGE_CELL/common/sgemaster -shadowd stop    # on every shadow host
   # $SGE_ROOT/$SGE_CELL/common/sgemaster -shadowd start
   # $SGE_ROOT/$SGE_CELL/common/sgeexecd softstop          # on every execution host
   # $SGE_ROOT/$SGE_CELL/common/sgeexecd start
   ```

   Running jobs are not affected by the restart of `sge_execd`. If `sge_execd` runs as a systemd
   service, it starts the `sge_shepherd` of each job in the separate systemd scope
   `<slice_name>-shepherds.scope`, so stopping the service does not end the jobs. After the restart,
   `sge_execd` takes over the jobs that are still running and reports them when they end.

   After the restart, new jobs can be submitted again.

8. Verify that the new version is in use. `qconf -help` prints the version of the commands in its
   first line. Each daemon logs its version at startup in its messages file, e.g.

   ```
   starting up xxQS_NAME_Sxx 9.1.7 (lx-amd64)
   ```

9. Back up your cluster again.

10. When the last job that was running before the patch installation has finished, remove the old
    binaries and libraries.

    ```
    # cd $SGE_ROOT
    # rm -rf bin.old lib.old utilbin.old
    ```

Other components, e.g. DBWriter, are updated as described in their own chapters, see chapter *Update
DBWriter*.

### Rollback of a Patch Installation

As long as the old binaries and libraries exist, a patch installation can be reverted. As user root,
swap the directories back, then restart the daemons as described in step 7:

```
# cd $SGE_ROOT
# mv bin bin.new && mv bin.old bin
# mv lib lib.new && mv lib.old lib
# mv utilbin utilbin.new && mv utilbin.old utilbin
```

The architecture-independent files of the patch release, e.g. scripts and templates, are not
reverted. To revert them as well, restore them from the backup made in step 1.

## Side-by-Side Upgrade

The least disruptive way to install a new minor or major version of xxQS_NAMExx is to install the
new version side-by-side with the old version, using the configuration of the old cluster to set up
the new cluster.

* Old jobs can finish in the old cluster, and new jobs can be submitted to the old cluster during
  the upgrade.
* You can test the new installation without affecting the old cluster.
* You can switch back to the old version at any time in case of problems.

The side-by-side upgrade does not move or clone jobs or advance reservations from the old cluster to
the new cluster.

The upgrade is done with the following steps:

1. Back up your cluster, see chapter *Backup and Restore*.

2. Download the new version of the software from the xxQS_COMPANY_NAMExx download page, verify the
   checksums as described in chapter *Download*, and read the release notes. If the release notes do
   not mention additional or different steps, continue with these instructions.

3. The following settings conflict with your old installation. Decide on new values before starting
   the upgrade:

   - Installation location (\$SGE_ROOT)
   - Cell name (\$SGE_CELL)
   - Cluster name (\$SGE_CLUSTER_NAME)
   - Port numbers of the master and execution services (\$SGE_QMASTER_PORT, \$SGE_EXECD_PORT)
   - Spool directories of the master and execution services
   - Group ID range used for job tracking
   - Next job ID to be used in the new cluster
   - Next advance reservation ID to be used in the new cluster
   - Name of the systemd slice used for resource management (if applicable)
   - Spooling database, if the old cluster uses PostgreSQL spooling, see chapter *PostgreSQL
     Spooling*

4. Log in to the master host as root and save all configuration files and objects of the old
   cluster:

   ```
   # cd $SGE_ROOT
   # ./util/upgrade_modules/save_config.sh <directory>
   ```

   The specified directory will contain a snapshot of your cluster configuration. Changes made to
   the old cluster after this point in time will not be part of the new cluster setup.

5. Unpack the new version of the software into the new \$SGE_ROOT directory and set the ownership
   and the file permissions of the new files, as described in chapter *Download*.

6. Start the upgrade as root by running the following commands in the new cluster:

   ```
   # export SGE_ROOT=<new_installation_location>
   # cd $SGE_ROOT
   # ./inst_sge -upd
   ```

   The upgrade procedure asks for the new configuration settings defined in step 3. It also asks for
   the location of the old configuration files and objects saved in step 4.

   At the end of this step, the new `sge_qmaster` process is active.

7. Set up the execution hosts. This step initializes each execution daemon's spool directory and
   installs the new autostart (RC or systemd) scripts. It does *not* start `sge_execd`; you start
   the daemons in a separate step below.

   By default, `inst_sge -upd-execd` and `inst_sge -upd-rc` are cluster-wide operations: they read
   the list of execution hosts from `qconf -sel` and reach every one of them over ssh. There are two
   ways to run this step:

   * **Option A: run it once from the master host.** If root can ssh from the master host to every
     execution host without a password prompt, run the commands on the master host:

     ```
     # . $SGE_ROOT/$SGE_CELL/common/settings.sh
     # $SGE_ROOT/inst_sge -upd-execd
     # $SGE_ROOT/inst_sge -upd-rc
     ```

     `inst_sge -post-upd` runs both commands in one step.

   * **Option B: run it locally on every execution host.** If passwordless root ssh is not
     configured, log in to each execution host as root and add the `-noremote` flag, which limits
     the action to the local host and suppresses all remote calls:

     ```
     # . $SGE_ROOT/$SGE_CELL/common/settings.sh
     # $SGE_ROOT/inst_sge -upd-execd -noremote
     # $SGE_ROOT/inst_sge -upd-rc -noremote
     ```

   Once the spool directories and startup scripts are in place, start the execution daemon on each
   execution host, either through the newly installed service (`systemctl start <slice_name>-execd`)
   or by running `$SGE_ROOT/$SGE_CELL/common/sgeexecd start`. Note that the resources of the hosts
   may become oversubscribed if new jobs are submitted to the new cluster while jobs of the old
   cluster are still running on the same hosts.

8. Set up the shadow master hosts. Skip this step if your cluster does not use shadow masters, or if
   every shadow master host is also an execution host. In the latter case, step 7 already installed
   the qmaster/shadowd startup scripts on that host, because `inst_sge -upd-rc` installs both the
   qmaster and the execd startup scripts on the local host.

   Otherwise, on every shadow master host that is *not* also an execution host, log in as root and
   run:

   ```
   # . $SGE_ROOT/$SGE_CELL/common/settings.sh
   # $SGE_ROOT/inst_sge -upd-rc -noremote
   ```

   You can skip logging in to each shadow master host if you chose Option A in step 7: `inst_sge
   -upd-rc` without `-noremote` already contacts every host listed in
   `$SGE_ROOT/$SGE_CELL/common/shadow_masters` over ssh and installs the startup scripts there.

   As with the execution daemons, this step does *not* start `sge_shadowd`. Start it on each shadow
   master host, either through the installed service (`systemctl start <slice_name>-qmaster`) or by
   running `$SGE_ROOT/$SGE_CELL/common/sgemaster -shadowd start`.

9. Check your new cluster.

   * Verify the version of the new cluster as described in step 8 of [Patch
     Installation](#patch-installation).
   * Submit some test jobs and check that they run as expected.
   * Make sure that the new cluster does not use user-provided scripts (JSV, prolog, epilog, PE
     start/stop, starter/suspend/resume methods, ...) in the old \$SGE_ROOT directory. Move them to
     a location outside the old and the new \$SGE_ROOT directories and reconfigure your cluster to
     use the new location.

10. When you are satisfied with the new cluster, switch over to it: disable job submission to the
    old cluster, e.g. with a server JSV that rejects all jobs, and let users source
    `$SGE_ROOT/$SGE_CELL/common/settings.sh` (or `settings.csh`) of the new cluster instead of the
    old one.

11. When the last job of the old cluster has finished, shut down the daemons of the old cluster and
    disable their startup scripts on all hosts, so that they are not started again when a host
    reboots. If the daemons of the old cluster run as systemd services, stop and disable them with
    `systemctl disable --now <old_slice_name>-qmaster` and `systemctl disable --now
    <old_slice_name>-execd`. Then remove the old \$SGE_ROOT directory.

## In-Place Upgrade

The in-place upgrade installs the new version without changing the installation location or other
key configuration parameters. Its downside is that you have to:

* empty the cluster by removing all jobs and disabling the submission of new jobs,
* stop all daemons during the upgrade,
* replace all binaries and libraries with the new version.

Compared to the side-by-side upgrade, the in-place upgrade is more disruptive, because all jobs have
to be removed and the cluster is unavailable during the upgrade.

The in-place upgrade is done with the following steps:

1. Back up your cluster, see chapter *Backup and Restore*.

2. Download the new version of the software from the xxQS_COMPANY_NAMExx download page, verify the
   checksums as described in chapter *Download*, and read the release notes. If the release notes do
   not mention additional or different steps, continue with these instructions.

3. Disable the cluster, e.g. by configuring a server JSV that rejects all jobs.

4. Wait for all jobs to complete, or delete all jobs.

5. Save the configuration of the old cluster. The directory has to be outside \$SGE_ROOT, otherwise
   it is removed in step 7.

   ```
   # cd $SGE_ROOT
   # ./util/upgrade_modules/save_config.sh <directory>
   ```

6. Shut down the execution, shadow, and master daemons on all hosts.

   ```
   # qconf -ke all
   # $SGE_ROOT/$SGE_CELL/common/sgemaster -shadowd stop    # on every shadow host
   # qconf -km
   ```

7. Delete the old subdirectories in \$SGE_ROOT except for your \$SGE_CELL directory.

   Make sure there are no user-provided scripts (JSV, prolog, epilog, PE start/stop,
   starter/suspend/resume methods, ...) in the \$SGE_ROOT directory that are still needed.

8. Extract the new software packages in \$SGE_ROOT and set the ownership and the file permissions of
   the new files, as described in chapter *Download*.

9. Start the upgrade.

   ```
   # cd $SGE_ROOT
   # ./inst_sge -upd
   ```

   The upgrade procedure asks several questions about configuration settings. It also asks for the
   location of the configuration saved in step 5.

   At the end of this step, the new `sge_qmaster` process is active.

10. Set up the execution hosts. This step initializes each execution daemon's spool directory and
    installs the new autostart (RC or systemd) scripts. It does *not* start `sge_execd`; you start
    the daemons in a separate step below.

    As in the side-by-side upgrade, `inst_sge -upd-execd` and `inst_sge -upd-rc` are cluster-wide
    operations by default: they read the list of execution hosts from `qconf -sel` and reach every
    one of them over ssh. There are two ways to run this step:

    * **Option A: run it once from the master host.** If root can ssh from the master host to every
      execution host without a password prompt, run the commands on the master host:

      ```
      # . $SGE_ROOT/$SGE_CELL/common/settings.sh
      # $SGE_ROOT/inst_sge -upd-execd
      # $SGE_ROOT/inst_sge -upd-rc
      ```

      `inst_sge -post-upd` runs both commands in one step.

    * **Option B: run it locally on every execution host.** If passwordless root ssh is not
      configured, log in to each execution host as root and add the `-noremote` flag, which limits
      the action to the local host and suppresses all remote calls:

      ```
      # . $SGE_ROOT/$SGE_CELL/common/settings.sh
      # $SGE_ROOT/inst_sge -upd-execd -noremote
      # $SGE_ROOT/inst_sge -upd-rc -noremote
      ```

    Once the spool directories and startup scripts are in place, start the execution daemon on each
    execution host, either through the newly installed service (`systemctl start
    <slice_name>-execd`) or by running `$SGE_ROOT/$SGE_CELL/common/sgeexecd start`.

11. Set up the shadow master hosts. Skip this step if your cluster does not use shadow masters, or
    if every shadow master host is also an execution host. In the latter case, step 10 already
    installed the qmaster/shadowd startup scripts on that host, because `inst_sge -upd-rc` installs
    both the qmaster and the execd startup scripts on the local host.

    Otherwise, on every shadow master host that is *not* also an execution host, log in as root and
    run:

    ```
    # . $SGE_ROOT/$SGE_CELL/common/settings.sh
    # $SGE_ROOT/inst_sge -upd-rc -noremote
    ```

    You can skip logging in to each shadow master host if you chose Option A in step 10: `inst_sge
    -upd-rc` without `-noremote` already contacts every host listed in
    `$SGE_ROOT/$SGE_CELL/common/shadow_masters` over ssh and installs the startup scripts there.

    As with the execution daemons, this step does *not* start `sge_shadowd`. Start it on each shadow
    master host, either through the installed service (`systemctl start <slice_name>-qmaster`) or by
    running `$SGE_ROOT/$SGE_CELL/common/sgemaster -shadowd start`.

12. Verify the version of the cluster as described in step 8 of [Patch
    Installation](#patch-installation), submit some test jobs, and enable job submission again.

[//]: # (Each file has to end with two empty lines)

