# Supported Operating Systems, Versions and Architectures

The functionality of Gridware Cluster Scheduler and Open Cluster Scheduler is categorized into three primary areas:

- **Master/Shadow Functionality:** Core services responsible for cluster operation, including backup and failover capabilities.
- **Execution Services:** Services enabling workload execution within the cluster.
- **Admin/Submit Clients and APIs:** Applications or APIs used for cluster configuration, status queries, workload submission, state management, and runtime monitoring.

The table below outlines the supported operating systems, versions, and architectures for versions v9.0.x of Gridware Cluster Scheduler. Open Cluster Scheduler generally follows a similar structure, though certain components may offer reduced functionality within specific operational areas.

> **Note**
>
> The availability of functionality in a primary area does not guarantee that *all* features within that area are provided or supported. Certain features may require additional software for support or may not be supported at all, even if the core functionality of the area is available. Find more information in the `Release Notes`, `User Guide`, and `Admin Guide` of the respective version.

| Operating System | Version | Architecture | Master/Shadow Service | Execution Service | Admin/Submit Clients |
| :--------------- | :------ | :----------- |:---------------------:| :---------------: | :------------------: |
| macOS            | 14      | ARM64        |           -           |         -         |          b           |
| CentOS Linux     | 8       | ARM64        |           x           |         x         |          x           |
| Redhat Linux     | 8       | ARM64        |           x           |         x         |          x           |
| Rocky Linux      | 8       | ARM64        |           x           |         x         |          x           |
| Rocky Linux      | 9       | ARM64        |           x           |         x         |          x           |
| Raspbian Linux   | 11      | ARM64        |           x           |         x         |          x           |
| Raspbian Linux   | 12      | ARM64        |           x           |         x         |          x           |
| Ubuntu Linux     | 24.04   | ARM64        |           x           |         x         |          x           |
|                  |         |              |                       |                   |                      |
| Alma Linux       | 8       | ppc64le      |           a           |         x         |          x           |
| Centos Linux     | 8       | ppc64le      |           a           |         x         |          x           |
| Rocky Linux      | 8       | ppc64le      |           a           |         x         |          x           |
|                  |         |              |                       |                   |                      |
| Alma Linux       | 8       | s390x        |           a           |         x         |          x           |
| Centos Linux     | 8       | s390x        |           a           |         x         |          x           |
| Rocky Linux      | 8       | s390x        |           a           |         x         |          x           |
|                  |         |              |                       |                   |                      |
| SUSE Tumbleweed  |         | Risc-V64     |           a           |         x         |          x           |
|                  |         |              |                       |                   |                      |
| Alma Linux       | 8       | x86-64       |           x           |         x         |          x           |
| Alma Linux       | 9       | x86-64       |           x           |         x         |          x           |
| CentOS Linux     | 6       | x86-64       |           -           |         d         |          d           |
| CentOS Linux     | 7       | x86-64       |           a           |         x         |          x           |
| CentOS Linux     | 8       | x86-64       |           x           |         x         |          x           |
| CentOS Linux     | 9       | x86-64       |           x           |         x         |          x           |
| Free BSD         | 13      | x86-64       |           a           |         x         |          x           |
| Free BSD         | 14      | x86-64       |           a           |         x         |          x           |
| Redhat Linux     | 6       | x86-64       |           a           |         x         |          x           |
| Redhat Linux     | 7       | x86-64       |           a           |         x         |          x           |
| Redhat Linux     | 8       | x86-64       |           x           |         x         |          x           |
| Redhat Linux     | 9       | x86-64       |           x           |         x         |          x           |
| Rocky Linux      | 8       | x86-64       |           x           |         x         |          x           |
| Rocky Linux      | 9       | x86-64       |           x           |         x         |          x           |
| Solaris          | 11      | x86-64       |           a           |         a         |          x           |
| OmniOS           | 11      | x86-64       |           a           |         a         |          a           |
| OpenIndiana      | 11      | x86-64       |           a           |         a         |          a           |
| SUSE Leap Linux  | 15      | x86-64       |           x           |         x         |          x           |
| SUSE Tumbleweed  |         | x86-64       |           a           |         x         |          x           |
| Ubuntu Linux     | 20.04   | x86-64       |           x           |         x         |          x           |
| Ubuntu Linux     | 22.04   | x86-64       |           x           |         x         |          x           |
| Ubuntu Linux     | 24.04   | x86-64       |           x           |         x         |          x           |

**x**: Supported. 

**-**: Unavailable and Unsupported.
 
**a**: Available but not covered by support contract. Contact our sales and support team if you need this configuration.

**b**: Still in beta testing. Contact our sales and support team if you need this configuration.

**d**: Deprecated. Will be removed in the next major release. 


> **Do You Need Help?** 
> 
> Is your operating system missing from the table above? This simply indicates that we have not completed all the necessary QA checks, not that Gridware Cluster Scheduler or Open Cluster Scheduler cannot be installed. If you want support coverage also for older operating systems then please reach out to our sales and support team for further assistance.

[//]: # (Each file has to end with two empty lines)

