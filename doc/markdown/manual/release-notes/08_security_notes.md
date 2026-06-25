# Security Notes

As part of the ongoing security work for the Cluster Scheduler, version 9.2.0 incorporates the results of an
internal security review of the core components. A number of defensive hardening fixes were applied, including:

- Hardening of client-reachable code paths so that malformed or oversized input is rejected cleanly instead of
  crashing the affected client or daemon (denial-of-service hardening).
- Correction of memory-safety defects identified during the review, including an out-of-bounds write.
- Tightening of the file permissions of qmaster spool files to the owning administrative account.

Where these fixes change externally visible behaviour, the details are described in the *Compatibility Notes*.
Users are encouraged to upgrade to benefit from the improved security posture.

[//]: # (Each file has to end with two empty lines)

