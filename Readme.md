# Open Cluster Scheduler

We are pleased to announce that we will continue the development of the Univa Open Core Grid Engine, which in 
turn is based on the open source Sun Grid Engine originally published by Sun Microsystems. Renamed 
"Open Cluster Scheduler", this project will remain open source under the SISSL v2 licence, with the source code 
available here.

In addition to maintaining the open source version, we will introduce advanced features under the name 
"Gridware Cluster Scheduler". This iteration will be offered with commercial support and consulting services from 
HPC-Gridware GmbH. The new features will be released under the Apache Licence v2.0.

## Choosing the Code Base

After careful consideration, we have selected the Univa code base as our foundation. Despite the availability of 
alternatives like Open Grid Scheduler and Some/Son of Grid Engine, the Univa version aligns best with our goal of 
making significant codebase improvements. Our inaugural release — version 9.0.0 — will mark a modernization effort, 
aligned with the ongoing growth in HPC and AI workloads which demand faster, more flexible computing clusters.

Technological advancements in CPU and GPU design necessitate advanced decision-making algorithms for schedulers. 
We aim to tackle these challenges, ensuring the Cluster Scheduler codebase can effectively support modern 
multi-core CPUs, GPUs, NPUs, and FPGAs.

## Preparing for the Future

Our immediate focus is on laying a robust foundation for the future of Cluster Scheduler. We’ll be:

* Convert the code base to C++ and CMake.
* Support modern development environments (e.g. CLion).
* Updating internal data stores and threading mechanisms.
* Enhancing concurrent execution within the master service for better thread parallelism.

These updates will be part of the initial “Open Cluster Scheduler” release. Key enhancements planned for version 9.0.0 
will include:

* RSMAPs for host-specific resource management, such as GPUs and other accelerators.
* Integration with the hwloc library for hardware topology and architecture analysis.
* Support for diverse computer architectures such as Linux on Intel/AMD64 (lx-amd64),
  AArch64 (lx-arm64), OpenPower (lx-ppc64le), RISC-V (lx-riscv64), 
  Apple’s ARM-based CPUs (darwin-arm64),
  Solaris on Intel/AMD64 (sol-amd64),
  and FreeBSD for Intel/AMD64 (fbsd-amd64).
* Enhanced online usage reporting and customizable accounting values.

## Streamlining the System

Implementation of request limits to guard against denial-of-service attacks.
Container based builds.

To further improve usability and maintainability, we plan to remove outdated or rarely-used components:

* Discontinuing the old Motif-based GUI.
* Removing qtcsh, in line with other commercial schedulers.
* Phasing out complex components like the JGDI interface and its associated services.
* Temporarily suspending CSP mode due to low user adoption.

While these changes might seem disruptive, they are essential for modernizing the system. We recognize the need for 
alternatives to these functionalities and commit to providing replacements in the future.

## Join the Conversations

We would love for you to join us in this exciting project! Please share your questions, suggestions, or interest in 
contributing to this project. You can reach out to us via the public users Google Group. If you are just curious about 
the project's progress, you can subscribe to the Git Google Group for our project.

* user+subscribe@clusterscheduler.org
* git+subscribe@clusterscheduler.org

## Commercial Offering

If you're ready to test the Gridware Cluster Scheduler, which is available with extensions and for which we provide 
commercial support, then get in touch! We can't wait to hear from you!

* sales@hpc-gridware.com
* [HPC-Gridware](https://www.hpc-gridware.com/)

## Other Documents

* [Build Instructions](https://github.com/hpc-gridware/clusterscheduler/blob/master/doc/markdown/manual/development-guide/01_prepare_dev_env.md)
* [Installation Guide](https://github.com/hpc-gridware/clusterscheduler/blob/master/doc/markdown/manual/installation-guide/01_planning_the_installation.md)

## License

The original files contributed by Sun Microsystems are licensed under

* [Sun Industry Standards Source License v1.2](https://github.com/hpc-gridware/clusterscheduler/blob/master/License_SISSL_v1-2.txt)

Newer contributions from HPC-Gridware GmbH (and others like Univa Inc. or individual contributors) are licensed under

* [Apache License v2.0](https://github.com/hpc-gridware/clusterscheduler/blob/master/License_APACHE_v2-0.txt)

Distribution packages might contain support for 3rd-party tools and libraries. License Information for corresponding
components is available under

* [3rd Party License Information](https://github.com/hpc-gridware/clusterscheduler/blob/master/source/dist/3rd_party/3rd_party_licscopyrights.md)

There is NO WARRANTY, to the extent permitted by law.
