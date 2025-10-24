# Enabling TLS Encryption

xxQS_NAMExx allows encrypting all communication between its components using TLS (Transport Layer Security).
This ensures that all data exchanged between the components is secure and protected from eavesdropping or
tampering.
Enabling TLS encryption requires that the cluster is set up with `security_mode` set to `tls` in the bootstrap file.
This requires a re-start of all xxQS_NAMExx components.

TLS encryption is supported on all platforms where OpenSSL version 3 or higher is available. It is not supported on
* `ulx-*` platforms (unsupported Linux, e.g. CentOS 7)
* `xlx-*` platforms (even older unsupported Linux, e.g. CentOS 6)

## Prerequisites for using TLS encryption

OpenSSL version 3 must be installed on all hosts within the security realm (the cluster).

OpenSSL is not delivered with the xxQS_NAMExx installation package and must be installed separately, ideally from your
OS package manager, which will ensure that all dependencies are met and that security updates are applied regularly.

### Installing OpenSSL 3

Installing OpenSSL 3 depends on your operating system. Below are instructions for some common Linux distributions.

#### Installing OpenSSL 3 on Alma/CentOS/RHEL/Rocky 8
```bash
dnf install -y openssl3
```

#### Installing OpenSSL 3 on FreeBSD 13

On FreeBSD 13 there is an openssl3 package, but it seems to be lacking libcrypto, which is required for
encryption.

Openssl 3.x can be built from source, and the built libraries can be used for both building and operating xxQS_NAMExx
in TLS mode.

```bash
# get the openssl-3.x source code and unpack it
# cd into the openssl source directory
./Configure
make
# as user root
make install
```

#### Other operating systems

On newer Linux distributions (e.g., Ubuntu 24.04, Alma/CentOS/RHEL/Rocky 9 and higher, SUSE Leap / SLES)
OpenSSL 3 is already the default version, and no additional installation is required.

Similarly, OpenSSL 3 is included in Solaris 11.4 and higher as well as in IllumOS / OpenSolaris / OpenIndiana,
in FreeBSD 14 and higher.

[//]: # (Each file has to end with two empty lines)

