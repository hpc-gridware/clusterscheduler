# Planning the Installation

Open Grid Engine is a workload management system used in data centres to connect tens to tens of thousands of computers 
into a compute cluster. 
It acts as the operating system of this cluster, connecting the underlying hardware according to user-configurable 
policies, scheduling its use, and providing computing power.
This section provides an overview of the Open Grid Engine architecture to assist with planning the cluster setup for 
your environment. The following questions are included to aid in decision-making and installation.

## UNIX Services

Open Grid Engine is a distributed software system that requires a number of services to run on different hosts in a 
network. When these services are running, users of a cluster can interact with these service components to submit 
workloads to the system as jobs, monitor their execution, and finally retrieve the computed results of these jobs.
A number of generic UNIX services are recommended, which will avoid problems and configuration effort if they are 
configured and tested in advance, before the Open Grid Engine is installed.

### Host Names

For the Open Grid Engine service components and all client applications, it is essential that hosts have a correct 
hostname setup. This requires each host to have a unique name for identification and communication purposes.

It is recommended to centrally manage hostnames using a DNS service or directory service like LDAP or NIS, rather 
than on a per-host basis using the */etc/hosts* file. This ensures consistent availability of hostnames across all 
hosts in a cluster.

Additionally, IP addresses for hostnames must be static. Primary hostnames should be fully qualified and aliases can be 
defined as needed. Each participating host must have *localhost* pointing to the IPv4 address *127.0.0.1*, or *::1* if 
IPv6 is enabled. It *must not* point to a specific NIC.

If a host has multiple NIC's, it is recommended that the primary hostname of each host points to the same NIC on all
cluster hosts. If this is not possible, *host aliasing* must be enabled for Open Grid Engine after the installation 
process is complete.

### User Account and Group Configuration

Using a directory service, such as LDAP or NIS, is recommended for configuring, managing, and sharing user and group 
information across the compute cluster.

### Network Services

To avoid manual changes to */etc/services* on each host, it is recommended to set up network services using a 
directory service such as LDAP or NIS.

Open Grid Engine suggests using predefined port numbers for the master service and execution service:

	sge_qmaster 6444/tcp
	sge_execd 6445/tcp

### File Systems

The installation of Open Grid Engine binaries, libraries, documentation and scripts can be done on a shared file 
system such as NFSv3, NFSv4 or Lustre.

Each cluster has a cell directory containing details of the cluster setup. The information in this directory must be 
accessible to all service components and client applications on all participating hosts.

There is also a spooling area where the master component makes dynamic data persistent. For a
high availability setup, this area also needs to be on a shared file system. If you want to improve the performance of 
your setup and are not planning to set up high availability, it is recommended that you use a local file system 
for spooling.

## Product Component Overview

The image below displays the components of a complete Open Grid Engine setup. The subsequent sections provide 
detailed explanations of these components.

TODO: image

### Master Service

The master service of an Open Grid Engine cluster is a central component that stores information about connected hosts 
and services, their configuration, and state. It is the brain of the cluster, knowing both the available computing 
resources of all hosts (such as GPU, CPU, memory, hard disk, and software licenses) and the requirements of jobs 
submitted by users. The master service's scheduling algorithm dispatches jobs to the necessary resources within 
the cluster.

Each cluster must have one master service running on a single host. Although it is possible to share the master 
machine's host resources with other services, it is not recommended. The master service should run exclusively on 
one host to avoid interfering with other services. It is crucial to prevent memory paging on the master machine as 
it can greatly reduce cluster performance.

The master service requires a minimum of *128MB memory, but may need up to 32GB depending on the number of compute 
tasks in the cluster. The master service requires a minimum of 2 CPU cores, with 4 being recommended. Additional CPU 
cores may be utilized if available, depending on the size, workload, and product version. A fast and reliable network 
connection is essential for the master service to connect to all other components running on hosts within this cluster.

### Shadow Service

Cluster hosts can optionally have shadow services installed to support the functionality of the master service in a 
high availability setup. A shadow service monitors the cluster and, if the active master host fails, restarts a 
master service on the shadow host. If multiple hosts have the shadow service, only one will restart the master 
service in the event of failure, while the others continue to provide backup functionality.

To make a high availability cluster setup, ensure that the shadow services have read and write access to the 
installation's cell directory. This directory contains the details of the cluster setup required to monitor the 
cluster operation. The shadow service needs full access to the master service's spool directory to ensure that a 
restarted master service can pick up where it left off. 

The minimum memory requirement for the shadow service is *128MB*. In the event of failover, the shadow host must meet 
the same requirements as a host running the master service.

### Execution Service

Each host that is to make its resources available to the compute cluster for use by compute jobs must have an 
execution service running on it. The service collects status information about the host it is running on and 
sends it to the master service. The execution service also receives workloads from the master service, which 
it can then execute.

At least one host running the execution service is required in a cluster to process workloads in the cluster. 

The service requires access to certain files in the cell directory, so the execution service must have at least 
read access to this directory. 

Before a host can run an execution service, it must be registered as an admin host, which is allowed to run 
administrative commands. The step to declare admin hosts is usually done as part of the installation of the 
master service. You can prepare a file containing all the primary host names of the execution hosts, or you can 
specify them individually when installing the master service.

### Administrative Commands

Administrative commands can be triggered from any host registered as an admin host in the Open Grid Engine system.

A cluster must have at least one admin host. The master host is automatically an admin host after a default 
installation. Admin hosts generally do not need to run a service component. 

Users with the manager role can add and remove admin hosts in a cluster.

### User Commands

Users wishing to submit workloads to a cluster must do so on hosts registered as submit hosts in the 
Open Grid Engine system.

A cluster must have at least one submit host. The master host is automatically mad a submit host during a 
default installation. There is no need to run a service component for submit hosts in general.

Users with the manager role can add and remove submit hosts in a cluster.

## User Roles

The Open Grid Engine uses Linux/Unix user account information to check whether a person has access to a certain 
functionality. The user must have identical usernames on all hosts that allow them to interact with the system 
(submit and admin hosts) and on hosts that run the execution service (execution hosts).

Within the Open Grid Engine system, Linux/Unix users can be assigned a specific role (operator/manager role) 
which then defines the access rights of that user.

### Root / Admin-User

The Open Grid Engine software must be installed as root. 

An admin-user must be specified during the installation process. Service components are then started as the root 
user and subsequently drop privileges to match those of the admin-user. 

Files and directories created during the installation process or by any of the OGE service components are owned 
by the admin-user.

Using the root user as the admin user is not recommended due to security concerns. Installing Open Grid Engine or 
locating one of the master spool areas on a shared network file system, where the root user typically 
has fewer privileges than a dedicated admin user account, can also cause problems.

### Managers

Administrative commands can be initiated by manager role user accounts from administrative hosts.

The list of manager usernames can be specified during installation or modified later by users with the manager role.

The admin user is automatically assigned the manager role.

### Operators

In order to perform certain administrative and user commands on behalf of others, the acting user account must 
have at least the Operator role. 

Users with the Manager role can customise the list of Operator usernames in a cluster.

### Users

Regular users can submit and monitor their own workload in an Open Grid Engine cluster. Additionally, 
regular users are permitted to execute certain administrative commands, provided that these commands 
do not alter the cluster configuration or disrupt general cluster operations.

To gain further access, a user must be assigned the operator or manager role, 
which requires manager privileges. 

## Installation Methods

The initial installation of an Open Grid Engine cluster can be performed manually or automatically without any 
intervention.

If you are installing OGE for the first time, it is recommended that you choose manual installation.

Automated installation can be helpful if you need to set up multiple clusters with slightly different parameters.
For this type of installation, a configuration file is created before the actual installation is performed. 
This file is used by the installer to set up a cluster without the need to answer any interactive questions.

## Information to be Prepared

Regardless of which installation method you choose, you need to answer a few questions according to your IT 
environment. The next sections will tell you these questions and your answers will be required during the 
installation of Open Grid Engine.

### What operating systems are installed on the cluster hosts?

Open Grid Engine supports the following operating systems.

TODO: Table

Before installation, check if the service you intend to install on a host is supported by the product packages for 
that platform.

### Where should OGE packages be installed?

The binaries, libraries, applications, scripts and documentation for Open Grid Engine are typically installed in 
a directory hierarchy located below an installation directory (e.g. */opt/oge*).

This installation directory can be on a shared network filesystem or on each participating host in the cluster 
individually. While local installation provides a performance benefit, central installation simplifies upgrades 
and maintenance.

All instructions in this document assume a shared installation location. Please note that additional steps are 
required to install the product packages on individual participating hosts, which are not mentioned in this document.

Once the installation is complete, the `\$SGE\_ROOT` environment variable will point to the installation directory.

### What is the name of the admin user?

The admin user should typically be distinct from the root user and have full access to the files in the 
installation directory. The username must be available on all hosts that will run service components. 
Services will be executed under the administrator user.

You must specify the username during the installation process.

### What is the name of the cell for this installation?

One product installation can be used to set up multiple compute clusters. Each cluster is identified by a 
unique cell name. It is recommended to choose the default cell name, *default*, if you only intend to set up 
one cluster.

If you choose a different name, ensure it is file system compliant with the installation directory. The name 
will be used as a directory name for the master's services configuration location.

During the installation process, it is necessary to specify the cell name.

Once the installation is complete, the cell name will be available as the value of the environment 
variable *\$SGE\_CELL*.

### What will be the unique name of the cluster?

Cluster names are used for service registration at the OS.

Cluster names must start with a letter followed by letters, digits, dashes or underscore characters. 
It is recommended to use the letter 'p' followed by the service port number of the master service (e.g. *p6444*).

You will need to specify the cluster name during the installation process.

Upon installation, the cell name will be accessible through the environment variable *\$SGE\_CLUSTER\_NAME.*

### Which spooling method should be used to make data persistent?

You can choose between classic spooling and BDB spooling.

BDB spooling may have performance advantages over classic spooling on non-SSD devices because write operations 
can be performed non-blocking to the database. However, classic spooling may be advantageous because it provides 
direct access to human-readable configuration files, especially during cluster downtimes.

During installation, it is important to specify a spooling method that is compatible with the network file systems 
where spooling operations will occur if setting up shadow hosts for a high availability (HA) setup. 
Please refer to the next question for more information.

### Where is the spooling area for the master service located?

For HA-setups, it must be a shared network location; otherwise, it can be the local filesystem of the host 
running the master service.

Ensure that the spooling location meets the requirements of the spooling mechanism. Classic spooling can be done on 
any shared filesystem, whereas BDB spooling can only be done on NFS v4 network locations.

By default, the installation process assumes that the installation directory is on a shared network location and 
suggests a subdirectory below the main installation directory.

You must specify the spooling location for the master service during installation.

### Where is the spooling area for execution services?

The installation of the master service suggests a location for the execution service, which is located below the 
installation directory. This location is *\$SGE\_ROOT/\$SGE\_CELL/spool/\<hostname\>*. 

If *\$SGE\_ROOT* is a shared network filesystem, it is recommended to change this during the execution service 
installation and specify a file system that is local to the corresponding execution node for performance reasons.

During the installation of the master service, you must specify the spooling location for the execution services. 

### Which hosts will act as the primary and shadow master hosts?

The master and all shadow master hosts require access to the installation directory, the cell directory, 
and the spooling area of the master service. 

The admin user account must be available on those hosts to execute the services.

Information about service ports must be available on that machine.

It would be advantageous to prepare a filename with the hostnames that should act as shadow hosts. The master 
installation process will ask for that file, but you can also specify filenames manually.

### Which hosts will run the execution service?

All execution services require access to the execution service spooling location, and the admin user account 
must be available to execute the services.

Information about service ports must be available on those machines.

Preparing a file with the hostnames that should act as execution hosts would be advantageous. During the master 
installation process, you will be prompted to provide a file. However, you also have the option to manually specify 
filenames.

### Which hosts will serve as administration and/or submission hosts?

Determine which hosts should allow administrative commands or job submission/observation. It is possible for hosts to 
serve as both administration and submission hosts. Hosts that do not serve as either administration or submission 
hosts cannot be used to execute Open Grid Engine commands.

During installation, you can specify a file containing hostnames that should be configured as administration hosts, 
but it is also possible to define these hosts manually if such a file is not present.

### What is the range of available GIDs for process tracking?

You need to specify an unused range of GIDs for monitoring and accounting of jobs. Each process that belongs 
to an executed job will be tagged with a unique supplementary GID, which allows for process filtering for 
specific jobs. GIDs used by the Open Grid Engine system to tag processes cannot be used for groups on hosts that 
run the execution service. 

The maximum number of jobs that can be run simultaneously on a single execution node depends on the number of 
available CPU cores. For optimal performance and utilization, it is recommended to specify a GID range that 
includes at least the same number of GIDs as the number of CPU cores.

For instance, if the execution host has 128 CPU cores, a maximum of 128 jobs should be started simultaneously. 
To assign a unique ID to each job, a range of GIDs, such as *20000-20128*, could be specified.

### Which scheduling profile should be used?

You can choose from three default scheduling profiles: *normal*, *high*, and *max*. After installation, 
you can change or adapt the selected profile to optimize your cluster setup.

The *normal* profile uses interval-based scheduling of jobs. The feature *load adjustments* and 
*scheduler job information* are enabled to collect and preserve information about each scheduling cycle for 
further analysis.

The scheduler is optimized for cluster throughput in the *high* profile, with interval-based scheduling enabled. 
However, other time-consuming features that could provide more insight into scheduling decisions are disabled.

The *max* profile further optimizes the scheduling profile for throughput. For short jobs that can run as soon as 
they enter a cluster or as soon as resources become available, interval-based scheduling is disabled. 

It is recommended to use the *high* profile and later adjust it for better performance or more data 
collection capabilities.

### Which installation method would you like to use?

If this is your first time installing Open Grid Engine, we suggest a manual installation.

Automatic installation is recommended if you need to install or reinstall a cluster multiple times or if you plan 
to install multiple clusters with slightly different settings.

