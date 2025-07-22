# Troubleshooting

## Auto-installation fails because \$SGE_ROOT/\$SGE_CELL already exists

It is an intended behaviour that the automatic installation fails if the \$SGE_ROOT/\$SGE_CELL directory already exists. This is to prevent accidental overwriting of an existing installation. If you wish to overwrite an existing installation then you must manually remove the \$SGE_ROOT/\$SGE_CELL directory before you start the automatic installation.

## Execution services are not running after automatic installation

Make sure that the passwordless ssh/rsh access to that remote host is configured correctly. The installer will try to start the execution service but on error it will continue with the installation of others. 

To solve this you can either fix the ssh/rsh problem and reinstall or start the execution service manually.

## Communication issues due to incorrect setup of hostnames

All xxQS_NAMExx services must be able to resolve the hostnames of all other machines part in the same cluster otherwise communication between the services or between clients and services will fail.

If you have a hostname resolution service (such as DNS, NIS, NIS+, LDAP, ...) in your network then make sure that all hostnames are correctly registered there and that all hosts are using it. If you do not have such a service then you have to make sure that all hostnames are correctly registered in the `/etc/hosts` file on all machines.

Also make sure that hostnames are not 'mapped' to loopback addresses (such as 127.0.0.1 or ::1 if IPv6 is enabled). Some systems have such default mappings in the `/etc/hosts` file which leads to communication issues even if you have a central hostname resolution service. 

To find and fix problems with hostname resolution you can use two utilities that are part of the xxQS_NAMExx software: `gethostname` and `gethostbyname`. Both utilities are located in the `$SGE_ROOT/utilbin/$ARCH` directory and, when started using the `-all` option they will display the primary hostname, alias names and IP addresses of the local machine or the machine with the specified hostname. They will also display the primary hostname as seen by xxQS_NAMExx components.

Here are examples for a correct setup:

```
$ $SGE_ROOT/utilbin/$ARCH/gethostname -all
Hostname: master_host.hpc-gridware.com
SGE name: master_host.hpc-gridware.com
Aliases:  master_host
Host Address(es): 10.1.1.1
```

and 

```
$ $SGE_ROOT/utilbin/$ARCH/gethostbyname -all master_host
Hostname: master_host.hpc-gridware.com
SGE name: master_host.hpc-gridware.com
Aliases:  master_host 
Host Address(es): 10.1.1.1
```

If you are experiencing communication problems then check the output of those commands on the master machine and the machine having the communication problem. Here are some of the more common problems:

* The hostname is not correctly assigned to the IP address
* The hostname is registered with a loopback address
* Primary hostname and aliases do not have the same sequence on all hosts
* Hostnames are assigned to different IP addresses on different hosts
* A host has multiple NIC's with different IP addresses

Most problems can be solved by correctly registering the hostnames in a directory service or `/etc/hosts` file. For setups where hosts have multiple NICs with different IP addresses, you need to make sure that xxQS_NAMExx knows about all IP addresses. This is done by defining a `host_aliases` file (see next section and sge_host_aliases(5)).

## IP Multipathing, Load Balancing or Bonding

If you are using IP multipathing, network load balancing, or certain bonding configurations on your master and/or execution nodes, you must ensure that the master and execution services are aware of the primary and additional IP addresses, otherwise communication between the services, or between clients and services, may fail completely or sporadically, depending on the network load.

Suppose you have a master host named *master_host* with the main network interface `eth0` and two additional network interfaces `eth1` and `eth2`, each with an assigned IP address. Upon installation, the master service will recognise the master host's main IP address and use it for communication. If you configure the underlying host system to use the additional NICs to achieve load balancing, then you must make xxQS_NAMExx aware of the additional IP addresses, otherwise communication from the unknown interfaces (`eth1` and `eth2`) will fail.

To solve this you need to define a `host_aliases` file (see sge_host_aliases(5)) to tell xxQS_NAMExx that the interfaces `eth0`, `eth1` and `eth2` are all part of the same host. You do this by specifying the assigned IP addresses in the `/etc/hosts` file or by defining the corresponding host names in the DNS/NIS or other directory services. 

```
10.1.1.1 master_host
10.1.1.2 master_host_eth1
10.1.1.3 master_host_eth2
```

As second step you have to define the `host_aliases` file to tell xxQS_NAMExx that the IP addresses are all part of the same host where the first mentioned name is the main name of the host.

```
master_host master_host_eth1 master_host_eth2
```

[//]: # (Eeach file has to end with two empty lines)

