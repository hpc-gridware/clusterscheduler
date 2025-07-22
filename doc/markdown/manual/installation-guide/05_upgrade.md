# Upgrade

The version string of the xxQS_NAMExx software has three different parts: the major version, the minor version, and the patch level. (e.g. v8.1.17 where 8 is the major version, 1 is the minor version, and 17 is the patch level).

The major and/or minor version of the Software will be upgraded if there are incompatible changes to the Software that require an upgrade procedure to be performed that makes necessary changes to configuration files or the database schema before the new version can be used. Alternatively, the software can be reinstalled.

If only the patch level is increased then this means that there are usually no incompatible changes. 

There are exceptions to those rules. Always check the release notes of the new version for details and back up your existing installation before starting the upgrade process. 

Please note that you cannot upgrade from Sun Grid Engine, Oracle Grid Engine, Some Grid Engine, Univa Grid Engine or Altair Grid Engine to xxQS_NAMExx but the installation steps of these products are almost identical to the installation steps of xxQS_NAMExx and a re-installation of the software is strongly recommended even if a side-by-side upgrade is at least partially possible.

If you have questions then please contact our support team directly or send a mail to [support@hpc-gridware.com](mailto:support@hpc-gridware.com).

## Patch installation

Patch installation is normally done by downloading the packages from the xxQS_COMPANY_NAMExx download page and unpacking them into the installation directory during a cluster downtime, but it is also possible to install the patches with minimal downtime by following the steps below.

1. Backup your cluster.

2. Download the patch packages from xxQS_COMPANY_NAMExx and read the release notes. If there are no specific steps mentioned in the release notes, you can keep all jobs and services running, but be aware that no new jobs can be submitted and no services can be restarted until the next three steps have been completed.

3. Move but do *NOT* remove binaries and libraries. It is important that the files remain on the same filesystem.

   ```
   $ cd $SGE_ROOT
   $ mv bin bin.old
   $ mv lib lib.old
   $ mv utilbin utilbin.old
   ```
   
   This ensures that running processed can still find the binaries and libraries if they need them.

4. Unpack the new packages in the $SGE_ROOT directory.

   ```
   $ tar xfz gcs-*.tar.gz
   ```

5. Trigger a restart of the services like `sge_qmaster`, `sge_execd`, and `sge_shadowd`.

   Now the new binaries and libraries are used by the services and new jobs can be submitted again. Old jobs still running are not affected by the patch installation. 

6. Ensure to create a new backup of the cluster.

7. After the last old job has finished you can remove the old binaries and libraries.

   ```
   $ rm -rf bin.old lib.old utilbin.old
   ```
   

## Side-by-Side Upgrade

The least disruptive way to install a new minor or major version of xxQS_NAMExx is to install the new version side-by-side with the old version using the configuration information of the old cluster to set up the new cluster. This way you can test the new version without affecting the old version.

* Old jobs can finish in the old cluster and also new jobs can be submitted to the old cluster during the upgrade process.
* You can test the new installation without affecting the old cluster.
* You can switch back to the old version at any time in case of problems.

Please note that the side-by-side upgrade does not move or clone your jobs or advance reservations from the old cluster to the new cluster.

The upgrade is done with following steps:

1. Backup your cluster.

2. Download the new version of the software from the xxQS_COMPANY_NAMExx download page and read the release notes. If there are no additional or different steps mentioned in the release notes then continue with these instructions.

3. The following list of settings will conflict with your old installation. You will need to decide on new values before starting the upgrade process: 

   - Installation location ($SGE_ROOT)
   - Cell name ($SGE_CELL)
   - Cluster name ($SGE_CLUSTER_NAME)
   - Port numbers for the master and execution services (\$SGE_QMASTER_PORT, \$SGE_EXECD_PORT)
   - Spool directories for the master and execution services
   - Group id range for used for job tracking

4. Login to your master machine as root and save all configuration files and objects of the old cluster by executing the following commands:

   ```
   $ cd $SGE_ROOT
   $ ./util/upgrade_modules/save_sge_config.sh <directory>
   ``` 
   
   The specified directory will contain a snapshot of your cluster configuration. Changes made to the old cluster after this point in time will not be part of the new cluster setup.

5. Unpack the new version of the software into the new $SGE_ROOT directory.

6. Start the upgrade process by running the following command in the new cluster:

   ```
   $ cd $SGE_ROOT
   $ ./inst_sge -upd 
   ```

   The upgrade procedure will ask you several questions about the new configuration settings defined in step 3. It will also ask you for the location of the old configuration files and objects created in step 4.

   At the end of this step your new `sge_qmaster` process will be active.

7. Update your execution environments. On each execution node you will need to source the new settings file and then trigger the initialization of the executions daemon spooling area and the startup scripts.

   ```
   $ . $SGE_ROOT/$SGE_CELL/common/settings.sh
   $ $SGE_ROOT/inst_sge -upd-execd
   $ $SGE_ROOT/inst_sge -upd-rc
   ```

   You can now start the execution daemon on the execution hosts now, but please be aware that the resources of the machines may become oversubscribed if you also immediately allow new jobs to be submitted to the new cluster.

8. Check your new cluster. 

   * Submit some test jobs and check that they are running as expected.
   * Make sure you do not have user generated scripts (JSV, Prolog, Epilog, PE-start/stop, starter/suspend/resume-method, ...) in the old $SGE_ROOT directory that are still used by the new cluster. Move them to a new location outside the old and new $SGE_ROOT directories and reconfigure your cluster to use the new location.

9. If you are satisfied with the new cluster then you can switch over using the new cluster.

10. If the last old job has finished then you can shut down the old daemons and remove the old $SGE_ROOT directory.

## In-Place Upgrade

The in-place upgrade allows you to upgrade the software without changing the installation location or other key configuration parameters but the downside of this upgrade method is that you have to:

* Empty the cluster by removing all jobs and disabling submission of new jobs
* Stop all services during the upgrade
* Replace all binaries and libraries with the new version

Compared to the side-by-side upgrade the in-place upgrade is more disruptive due to the need to remove all jobs and due to the unavailability of the cluster during the upgrade process.

Here are the steps required to complete the in-place upgrade:

1. Back up your cluster.

2. Download the new version of the software from the xxQS_COMPANY_NAMExx download page and read the release notes. If there are no additional or different steps mentioned in the release notes then proceed with these instructions.

3. Disable the cluster (e.g. by configuring a server JSV that rejects all jobs).

4. Wait for all jobs to complete or delete all jobs.

5. Save the configuration of the old cluster.

   ```
   $ cd $SGE_ROOT
   $ ./util/upgrade_modules/save_sge_config.sh <directory>
   ```
   
6. Shutdown execution, shadow and master services on all machines.

   ```
   $ qconf -ke all
   $ $SGE_ROOT/$SGE_CELL/common/sgemaster -shadowd stop
   $ qconf -km
   ```
   
7. Delete the old subdirectories in \$SGE_ROOT except for your \$SGE_CELL directory.
 
   Make sure there are no user generated scripts (JSV, Prolog, Epilog, PE-start/stop, starter/suspend/resume-method, ...) in the $SGE_ROOT directory that are still needed.

8. Extract the new software packages in $SGE_ROOT.

9. Start the upgrade process.

   ```
   $ cd $SGE_ROOT
   $ ./inst_sge -upd
   ```

   The upgrade process will ask you several questions about configuration settings.

   At the end of this step your new `sge_qmaster` process is active.

10. Upgrade your execution environments. On each execution node you need to source the new settings file and then trigger the initialization of the executions daemons spooling area and the startup scripts.

    ```
    $ . $SGE_ROOT/$SGE_CELL/common/settings.sh
    $ $SGE_ROOT/inst_sge -upd-execd
    $ $SGE_ROOT/inst_sge -upd-rc
    ```

    You can now start the execution daemon on the execution hosts.

11. Continue with post installation steps.

[//]: # (Eeach file has to end with two empty lines)

