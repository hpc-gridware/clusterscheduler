# Enabling TLS Encryption

xxQS_NAMExx allows to encrypt all communication between its components using TLS (Transport Layer Security).
This ensures that all data exchanged between the components is secure and protected from eavesdropping or
tampering.
Enabling TLS encryption requires that the cluster is set up with `security_mode` set to `tls` in the bootstrap file. This requires a re-start of all xxQS_NAMExx components.

TLS encryption is supported on all platforms where OpenSSL version 3 or higher is available. It is not supported on
* `ulx-*` platforms (unsupported Linux, e.g. CentOS 7)
* `xlx-*` platforms (even older unsupported Linux, e.g. CentOS 6)

## Prerequisites for using TLS encryption

OpenSSL version 3 must be installed on all hosts within the security realm (the cluster).

OpenSSL is not delivered with the xxQS_NAMExx installation package and must be installed separately, ideally from your OS package manager, which will ensure that all dependencies are met and that security updates are applied regularly.

### Installing OpenSSL 3

Installing OpenSSL 3 depends on your operating system. Below are instructions for some common Linux distributions.

#### Installing OpenSSL 3 on Alma/CentOS/RHEL/Rocky 8
```bash
dnf install -y openssl3
```

#### Installing OpenSSL 3 on FreeBSD 13

```bash
pkg install -y openssl3
```

#### Other operating systems

On newer Linux distributions (e.g. Ubuntu 24.04, Alma/CentOS/RHEL/Rocky 9 and higher, Suse 15.x @todo exact names) OpenSSL 3 is already the default version and no additional installation is required.

Similarly, OpenSSL 3 is included in Solaris 11.4 and higher as well as in IllumOS / OpenSolaris / OpenIndiana, in FreeBSD 14 and higher, and in macOS 11 (Big Sur) @todo verify and higher.
