# Installation

Once you have gathered the necessary information as outlined in previous chapters, you may proceed with the 
installation process for xxQS_NAMExx.

## Manual Installation

This section covers the manual installation process on the command line. Note the prerequisites are required as outlined in previous chapters. If the hostname setup, usernames and service configuration are correct for all hosts that you intend to include in you cluster, then you can continue with the installation the master service.

### Installation of the Master Service

During the execution the master's installation procedure following steps are processed.

* a cell directory will be created that will contain files that are read by all service components as well as client applications communicating with the service within that cell.

* next installation steps for other services are prepared (e.g. admin hosts are defined that will later on be allowed to run execution services).

* a default configuration is created and user specific changes are applied.

* the master service is started and basic tests of its functionality are executed.

* the master service is integrated into the launch environment of the operating system so that it is automatically started at boot time.

Here are the steps required to complete the installation.

1. Log in as user root on the master host.

2. Set the `$SGE_ROOT` environment variable. That variable has to point to the installation directory where all files have been installed that where part of the product packages. Then switch into that directory.

   ```
   # export SGE_ROOT=<install_dir>
   # cd $SGE_ROOT
   ```
   
3. Start the installation process by executing the `install_qmaster` script and read and follow the given instructions.   

   ```
   # ./install_qmaster
   ```
   In order to install with Munge authentication pass the `-munge` option to the installation script.

   ```
    # ./install_qmaster -munge
   ```
   
   Read and follow the given instructions.
   ```
   Welcome to the Cluster Scheduler installation
   ---------------------------------------------
 
   Cluster Scheduler qmaster host installation
   -------------------------------------------
 
   Before you continue with the installation please read these hints:
 
      - Your terminal window should have a size of at least
        80x24 characters
   
      - The INTR character is often bound to the key Ctrl-C.
        The term >Ctrl-C< is used during the installation if you
        have the possibility to abort the installation
   
   The qmaster installation procedure will take approximately 5-10 minutes.
   
   Hit <RETURN> to continue >>
   ```

4. Admin User: Either accept the suggested admin user or reject it. If you reject the suggestion then you can select a different one.

   ```
   Cluster Scheduler admin user account
   ------------------------------------ 
   
   The current directory
   
      /home/ebablick/OGE/oge1/inst
   
   is owned by user
   
      <admin_user>
   
   If user >root< does not have write permissions in this directory on *all*
   of the machines where Cluster Scheduler will be installed (NFS partitions not
   exported for user >root< with read/write permissions) it is recommended to
   install Cluster Scheduler that all spool files will be created under the user id
   of user >admin_user<.

   IMPORTANT NOTE: The daemons still have to be started by user >root<.

   Do you want to install Cluster Scheduler as admin user >admin_user< (y/n) [y] >> 
   ```
   
5. Installation Location: Specify the installation directory. The suggested default is the directory you set in installation step 2.

   ```
   Checking $SGE_ROOT directory
   ---------------------------- 
   
   The Cluster Scheduler root directory is:
   
      $SGE_ROOT = <installation_directory>
   
   If this directory is not correct (e.g. it may contain an automounter
   prefix) enter the correct path to this directory or hit <RETURN>
   to use default [<installation_directory>] >> 
   ```

6. Master Service Port: Specify which service port should be used for the master service. If you have an entry in */etc/services* or if a directory service is available that provides that information for `sge_qmaster` then the installer will show you the configured port number and use that as service port. Alternatively you can specify a different port number via shell environment.

   ```
   Cluster Scheduler TCP/IP communication service
   ----------------------------------------------
   
   The port for sge_qmaster is currently set as service.
   
       sge_qmaster service set to port 6444
 
   Now you have the possibility to set/change the communication ports by using the
   >shell environment< or you may configure it via a network service, configured
   in local >/etc/service<, >NIS< or >NIS+<, adding an entry in the form
   
       sge_qmaster <port_number>/tcp
  
   to your services database and make sure to use an unused port number.
  
   How do you want to configure the Cluster Scheduler communication ports?
   
   Using the >shell environment<:                           [1]
   
   Using a network service like >/etc/service<, >NIS/NIS+<: [2]
   
   (default: 2) >> 
   ```

7. Execution Service Port: Specify which service port should be used for the execution service. If you have an entry in */etc/services* or if a directory service is available that provides that information for `sge_execd` then the installer will show you the configured port number and use that as service port. Alternatively you can specify a different port number via shell environment.

   ```
   Cluster Scheduler TCP/IP communication service
   ----------------------------------------------
   
   The port for sge_execd is currently set as service.
   
       sge_execd service set to port 6445
   
   Now you have the possibility to set/change the communication ports by using the
   >shell environment< or you may configure it via a network service, configured
   in local >/etc/service<, >NIS< or >NIS+<, adding an entry in the form
  
       sge_execd <port_number>/tcp
  
   to your services database and make sure to use an unused port number.
  
   How do you want to configure the Cluster Scheduler communication ports?
   
   Using the >shell environment<:                           [1]
   
   Using a network service like >/etc/service<, >NIS/NIS+<: [2]
   
   (default: 2) >> 
   ```

8. Cluster Cell: Either confirm the *default* cell name for the cluster or specify a different name that does not collide with the cell names of other clusters that you might have installed previously.

   ```
   Cluster Scheduler cells
   -----------------------
   
   Cluster Scheduler supports multiple cells. 
   
   If you are not planning to run multiple Cluster Scheduler clusters or if you don't
   know yet what is a Cluster Scheduler cell it is safe to keep the default cell name
   
      default
   
   If you want to install multiple cells you can enter a cell name now.
   
   The environment variable
   
      $SGE_CELL=<your_cell_name>
   
   will be set for all further Cluster Scheduler commands.
   
   Enter cell name [default] >> 
   ```

9. Unique Cluster Name: Specify a unique cluster name that does not collide with the cluster names of other clusters that you might have installed previously.

   ```
   Unique cluster name
   -------------------
   
   The cluster name uniquely identifies a specific Cluster Scheduler cluster.
   The cluster name must be unique throughout your organization. The name 
   is not related to the SGE cell.
   
   The cluster name must start with a letter ([A-Za-z]), followed by letters, 
   digits ([0-9]), dashes (-) or underscores (_).
   
   Enter new cluster name or hit <RETURN>
   to use default [p6444] >> 
   ``` 

10. Master Spool Directory: Specify the spooling location for the master service. The suggested default will be a directory with your specified cell name within the installation directory.

    ```
    Cluster Scheduler qmaster spool directory
    -----------------------------------------
    
    The qmaster spool directory is the place where the qmaster daemon stores
    the configuration and the state of the queuing system.
    
    The admin user >ebablick< must have read/write access
    to the qmaster spool directory.
    
    If you will install shadow master hosts or if you want to be able to start
    the qmaster daemon on other hosts (see the corresponding section in the
    Cluster Scheduler Installation and Administration Manual for details) the account
    on the shadow master hosts also needs read/write access to this directory.
    
    Enter a qmaster spool directory [<installation_directory>/<cell_name>] >> 
    ```

11. Hostname Resolving: Specify if all hosts that should participate in the cluster are part of one single domain.

    ```
    Select default Cluster Scheduler hostname resolving method
    ---------------------------------------------------------- 
    
    Are all hosts of your cluster in one DNS domain? If this is
    the case the hostnames 
  
       >hostA< and >hostA.foo.com<
    
    would be treated as equal, because the DNS domain name >foo.com<
    is ignored when comparing hostnames. 
   
    Are all hosts of your cluster in a single DNS domain (y/n) [y] >> 
    ```

12. Creation of Master Spooling Location: The spool area will then be prepared.

    ```
    Making directories
    ------------------
    
    creating directory: <install_dir>/<cell_name>/spool/qmaster
    creating directory: <install_dir>/<cell_name>/spool/qmaster/job_scripts
    Hit <RETURN> to continue >> 
    ```

13. Choose Spooling Method: Select classic or BDB spooling. As part of this step the spooling file will be created.

    ```
    Setup spooling
    --------------
    Your SGE binaries are compiled to link the spooling libraries
    during runtime (dynamically). So you can choose between Berkeley DB 
    spooling and Classic spooling method.
    Please choose a spooling method (berkeleydb|classic) [classic] >>
    ```

14. Job Observation via GID's: Specify an available group ID range.

    ```
    Cluster Scheduler group id range
    --------------------------------
  
    When jobs are started under the control of Cluster Scheduler an additional group id
    is set on platforms which do not support jobs. This is done to provide maximum
    control for Cluster Scheduler jobs.
    
    This additional UNIX group id range must be unused group id's in your system.
    Each job will be assigned a unique id during the time it is running.
    Therefore you need to provide a range of id's which will be assigned
    dynamically for jobs.
  
    The range must be big enough to provide enough numbers for the maximum number
    of Cluster Scheduler jobs running at a single moment on a single host. E.g. a range
    like >20000-20100< means, that Cluster Scheduler will use the group ids from
    20000-20100 and provides a range for 100 Cluster Scheduler jobs at the same time
    on a single host.
  
    You can change at any time the group id range in your cluster configuration.
  
    Please enter a range [20000-20100] >> 
    ```

15. Execution Service Spooling Location: Specify a default spooling location that should be used by all execution nodes.

    ```
    Cluster Scheduler cluster configuration
    ---------------------------------------
    
    Please give the basic configuration parameters of your Cluster Scheduler
    installation:
  
       <execd_spool_dir>
  
    The pathname of the spool directory of the execution hosts. User >ebablick<
    must have the right to create this directory and to write into it. 
  
    Default: [<install_dir>/<cell_name>/spool] >> 
    ```

16. Administrator Mail: Specify the mail address that should receive administrator mail.

    ```
    Cluster Scheduler cluster configuration (continued)
    --------------------------------------------------- 
    
    <administrator_mail>
  
    The email address of the administrator to whom problem reports are sent.
  
    It is recommended to configure this parameter. You may use >none<
    if you do not wish to receive administrator mail.
    
    Please enter an email address in the form >user@foo.com<.
    
    Default: [none] >> 
    ```

17. Spooling Data will be written: The shown messages during that process depend on the selected spooling method.

    ```
    Creating local configuration
    ----------------------------
    Creating >act_qmaster< file
    Adding default complex attributes
    Adding default parallel environments (PE)
    Adding SGE default usersets
    Adding >sge_aliases< path aliases file
    Adding >qtask< qtcsh sample default request file
    Adding >sge_request< default submit options file
    Creating >sgemaster< script
    Creating >sgeexecd< script
    Creating settings files for >.profile/.cshrc<
   
    Hit <RETURN> to continue >> 
    ```

18. Autostart: Select if the master service should be integrated into the launch environment of the OS.

    ```
    qmaster startup script
    ----------------------
    
    We can install the startup script that will
    start qmaster at machine boot (y/n) [y] >> 
    ```

19. Service Start: Now the master service is started.

    ```
    Cluster Scheduler qmaster startup
    --------------------------------- 
    
    Starting qmaster daemon. Please wait ...
       starting sge_qmaster
    Hit <RETURN> to continue >> 
    ```

20. Host Permissions: Select the hosts that should later on run the execution service. Those host will be administration hosts and submit hosts automatically.

    ```
    Adding Cluster Scheduler hosts
    ------------------------------
    
    Please now add the list of hosts, where you will later install your execution
    daemons. These hosts will be also added as valid submit hosts.
    
    Please enter a blank separated list of your execution hosts. You may
    press <RETURN> if the line is getting too long. Once you are finished
    simply press <RETURN> without entering a name.
    
    You also may prepare a file with the hostnames of the machines where you plan
    to install Cluster Scheduler. This may be convenient if you are installing Grid
    Engine on many hosts. 
    
    Do you want to use a file which contains the list of hosts (y/n) [n] >> 
    ``` 

    If you have no file available you can also add those hostnames manually:

    ```
    Adding admin and submit hosts
    -----------------------------
    
    Please enter a blank seperated list of hosts. 
    
    Stop by entering <RETURN>. You may repeat this step until you are
    entering an empty list. You will see messages from Cluster Scheduler
    when the hosts are added.
    
    Host(s): 
    ```

    Optionally you also add your shadow hosts now as administrative hosts:
 
    ```
    Adding Cluster Scheduler shadow hosts
    -------------------------------------
    
    Please now add the list of hosts, where you will later install your shadow
    daemon. 
    
    Please enter a blank separated list of your execution hosts. You may
    press <RETURN> if the line is getting too long. Once you are finished
    simply press <RETURN> without entering a name. 
    
    You also may prepare a file with the hostnames of the machines where you plan
    to install Cluster Scheduler. This may be convenient if you are installing Grid
    Engine on many hosts. 
    
    Do you want to use a file which contains the list of hosts (y/n) [n] >> 
    ```

    Also, this can be done step by step if you have no prepared file with hostnames.

    ```
    Adding admin hosts
    ------------------
    
    Please enter a blank seperated list of hosts.
    
    Stop by entering <RETURN>. You may repeat this step until you are
    entering an empty list. You will see messages from Cluster Scheduler
    when the hosts are added.
    
    Host(s): 
    ```

21. Default Configuration Steps: Depending on you host setup and configuration steps some default configuration objects will be created for you cluster.

    ```
    Creating the default <all.q> queue and <allhosts> hostgroup
    -----------------------------------------------------------
   
    root@<hostname>.<domainname> added "@allhosts" to host group list
    root@<hostname>.<domainname> added "all.q" to cluster queue list
   
    Hit <RETURN> to continue >> 
    ```

22. Scheduler Configuration: Choose one of the predefined templates for the scheduler configuration.

    ```
    Scheduler Tuning
    ----------------
    
    The details on the different options are described in the manual. 
    
    Configurations
    --------------
    1) Normal
             Fixed interval scheduling, report limited scheduling information,
             actual + assumed load
   
    2) High
             Fixed interval scheduling, report limited scheduling information,
             actual load
   
    3) Max
             Immediate Scheduling, report no scheduling information,
             actual load
 
    Enter the number of your preferred configuration and hit <RETURN>! 
    Default configuration is [1] >> 
    ```

23. Installation Summary

    ```
    Using Cluster Scheduler
    -----------------------
    
    You should now enter the command:
   
       source <installation_directory>/<cell_name>/common/settings.csh
    
    if you are a csh/tcsh user or
    
       # . <installation_directory>/<cell_name>/common/settings.sh
    
    if you are a sh/ksh user. 
    
    This will set or expand the following environment variables:
   
       - $SGE_ROOT         (always necessary)
       - $SGE_CELL         (if you are using a cell other than >default<)
       - $SGE_CLUSTER_NAME (always necessary)
       - $SGE_QMASTER_PORT (if you haven't added the service >sge_qmaster<)
       - $SGE_EXECD_PORT   (if you haven't added the service >sge_execd<)
       - $PATH/$path       (to find the Cluster Scheduler binaries)
       - $MANPATH          (to access the manual pages)
  
    Hit <RETURN> to see where Cluster Scheduler logs messages >> 
    ```
 
    Note down the details how you can prepare to use the cluster.
 
    ```
    Cluster Scheduler messages
    --------------------------
    
    Cluster Scheduler messages can be found at:
    
       /tmp/qmaster_messages (during qmaster startup)
       /tmp/execd_messages   (during execution daemon startup)
    
    After startup the daemons log their messages in their spool directories.
  
       Qmaster:     <install_dir>/<cell_name>/spool/qmaster/messages
       Exec daemon: <execd_spool_dir>/<hostname>/messages
   
 
    Cluster Scheduler startup scripts
    ---------------------------------
    
    Cluster Scheduler startup scripts can be found at:
    
       <install_dir>/<cell_name>/common/sgemaster (qmaster)
       <install_dir>/<cell_name>/common/sgeexecd (execd)
 
    Do you want to see previous screen about using Cluster Scheduler again (y/n) [n] >>
    ```
   
    Should you have seen error messages during the installation then the mentioned message files will contain more details about them.
 
    ```
    Your Cluster Scheduler qmaster installation is now completed
    ------------------------------------------------------------ 
    
    Please now login to all hosts where you want to run an execution daemon
    and start the execution host installation procedure.
    
    If you want to run an execution daemon on this host, please do not forget
    to make the execution host installation in this host as well.
    
    All execution hosts must be administrative hosts during the installation.
    All hosts which you added to the list of administrative hosts during this
    installation procedure can now be installed. 
    
    You may verify your administrative hosts with the command
    
       # qconf -sh
   
    and you may add new administrative hosts with the command
    
       # qconf -ah <hostname>
    ```
 
    You are reaching the end of the manual installation.

### Installation of the Execution Service

During the execution host installation procedure following steps are processed:

* It is tested that the master service is running and that the execution host is able to communicate with the master service.

* An appropriate directory hierarchy is created as required by the `sge_execd` service.

* The `sge_execd` service is started and basic tests of its functionality are executed.

* The host is added to a default queue (optional)

Here are the steps required to complete the installation.

1. Log in as user root on an execution host.

2. Source the settings file that was created during the master service installation or set the SGE_ROOT environment variable manually. This Installation Guide assumes that the installation directory is available on all hosts in the same location.

   ```
   # . <install_dir>/<cell_name>/common/settings.sh
   # cd $SGE_ROOT
   ```
   
4. Verify, that the execution host has been declared as administrative host. Do this by executing the following `qconf` command on the master machine. The hostlist should contain the hostname of the new execution host. If it does not exit, then add the hostname to the list of administrative hosts by executing `qconf -ah <hostname>` on the master machine.

   ```
   # qconf -sh
   ...
   ```
5. Start the installation process by executing the `install_execd` script and read and follow the given instructions.

   ```
   # ./install_execd
   Welcome to the Cluster Scheduler execution host installation
   ------------------------------------------------------------
   
   If you haven't installed the Cluster Scheduler qmaster host yet, you must execute
   this step (with >install_qmaster<) prior the execution host installation.
   
   For a successful installation you need a running Cluster Scheduler qmaster. It is
   also necessary that this host is an administrative host.
   
   You can verify your current list of administrative hosts with
   the command:
   
   # qconf -sh
   
   You can add an administrative host with the command:

   # qconf -ah <hostname>
   
   The execution host installation will take approximately 5 minutes.

   Hit <RETURN> to continue >>
   ```
 
6. Confirm the installation directory. The suggested default is the directory you set in the master service installation.

   ```
   Checking $SGE_ROOT directory
   ----------------------------

   The Cluster Scheduler root directory is:

      $SGE_ROOT = <installation_directory>

   If this directory is not correct (e.g. it may contain an automounter
   prefix) enter the correct path to this directory or hit <RETURN>
   to use default [<installation_directory>] >> 
   ```
   
7. Confirm the cell directory. The suggested default is the directory you set in the master service installation. You can enter a different cell name if you intend to start the execution service in a different cell.

   ```
   Cluster Scheduler cells
   -----------------------

   Please enter cell name which you used for the qmaster
   installation or press <RETURN> to use [default] >>
   ```
   
8. Confirm the detected execution daemon TCP/IP port number.

   ```
   Cluster Scheduler TCP/IP communication service
   ----------------------------------------------

   The port for sge_execd is set as service.

   sge_execd service set to port 6445

   Hit <RETURN> to continue >>
   ``` 
   
9. The installer does verify the local hostname resolution and if the current host is an administrative host. 

   ```
   Checking hostname resolving
   ---------------------------

   This hostname is known at qmaster as an administrative host.

   Hit <RETURN> to continue >>
   
10. Specify the spooling directory for execution hosts

   ```
   Execd spool directory configuration
   -----------------------------------

   You defined a global spool directory when you installed the master host.
   You can use that directory for spooling jobs from this execution host
   or you can define a different spool directory for this execution host.
   
   ATTENTION: For most operating systems, the spool directory does not have to
   be located on a local disk. The spool directory can be located on a 
   network-accessible drive. However, using a local spool directory provides 
   better performance.
   
   The spool directory is currently set to:
   <<<installation_directory>/default/spool/<hostname>>>

   Do you want to configure a different spool directory
   for this host (y/n) [n] >>
   ``` 
   
11. The installer will create a local configuration for the execution host.

   ```
   Creating local configuration
   ----------------------------
   <admin_user>@<hostname> added "<hostname>" to configuration list
   Local configuration for host ><hostname>< created.

   Hit <RETURN> to continue >> 
   ```

12. Now specify if you want to start the execution service automatically.

   ```
   execd startup script
   --------------------
   
   We can install the startup script that will
   start execd at machine boot (y/n) [y] >> 
   ```
   
13. The execution service is started.

   ```
   Cluster Scheduler execution daemon startup
   ------------------------------------------

   Starting execution daemon. Please wait ...
      starting sge_execd

   Hit <RETURN> to continue >>
   ```
   
14. Specify a queue for the new host.

   ```
   Adding a queue for this host
   ----------------------------

   We can now add a queue instance for this host:

      - it is added to the >allhosts< host group
      - the queue provides 32 slot(s) for jobs in all queues
        referencing the >allhosts< host group

   You do not need to add this host now, but before running jobs on this host
   it must be added to at least one queue.

   Do you want to add a default queue instance for this host (y/n) [y] >>
   ```
   
## Automatic Installation

The automatic installation process is based on the manual installation process where the installer gets a configuration file with predefined answers to those questions that would normally be asked during an interactive installation. For an automatic installation the configuration file has to be prepared, and it has to be passed to the installation script as argument with the `-auto` option.

The auto installation is also able to install services on remote hosts if either passwordless `ssh` or `rsh` access is configured for the root user on the master machine. 

1. Login as root on the system where you intend to install a service.
 
2. Make of copy of a configuration template file and prepare it with the answers to the questions that are usually asked during the manual installation process. If the root user has no write permissions in $SGE_ROOT then choose a different path but make sure that you preserve the file for the uninstallation process.

   ```
   $ cp $SGE_ROOT/util/install_modules/inst_template.conf $SGE_ROOT/my_template.conf
   $ vi $SGE_ROOT/my_template.conf
   ...
   ```
  
3. On the master machine start the master installation

   ```
   cd $SGE_ROOT
   ./inst_sge -m -auto $SGE_ROOT/my_template.conf
   ```

   In order to install with Munge authentication pass the `-munge` option to the installation script.

   ```
   cd $SGE_ROOT
   ./inst_sge -munge -m -auto $SGE_ROOT/my_template.conf
   ```
   
4. If you have a list of hosts specified as EXEC_HOST_LIST parameter in the configuration file AND when you have passwordless `ssh` or `rsh` access to those hosts then you can install the execution service on those hosts remotely from the master machine.

   ```
   cd $SGE_ROOT
   ./inst_sge -x -auto $SGE_ROOT/my_template.conf
   ```
   
   If you have no passwordless `ssh` or `rsh` access to those hosts then you have to log in to each host and start the installation process manually for each host individually.

5. On shadow hosts install the shadow service

   ```
   cd $SGE_ROOT
   ./inst_sge -sm -auto $SGE_ROOT/my_template.conf
   ```

## Uninstallation

The uninstallation of the xxQS_NAMExx software can be done manually or automatically using the configuration template created during the auto installation. If you uninstall an execution host then make sure that there are no running jobs on that host. If you uninstall manually then make sure that all execution hosts are uninstalled first before you uninstall the master host or other services.

1. Login as root on the system where you installed a service.

2. Automatic uninstall the execution service on execution hosts.

   ```
   cd $SGE_ROOT
   ./inst_sge -ux -auto $SGE_ROOT/my_template.conf
   ```
   
3. Manual uninstallation of the execution component.

   ```
   cd $SGE_ROOT
   ./inst_sge -ux
   ```
   
4. Qmaster, shadow master and other services can be uninstalled the same way. To uninstall the qmaster service use the `-um` switch, for the shadow master service use the `-usm` switch. For the automatic uninstallation use the `-auto` switch with the configuration template.

   ```
   cd $SGE_ROOT
   ./inst_sge ...  
   ```

[//]: # (Eeach file has to end with two emty lines)

