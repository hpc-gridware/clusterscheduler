# Compatibility Notes

## Binding

With the introduction of the new advanced binding framework in version 9.1.0, there are important 
compatibility considerations to be aware of:

- **Legacy Binding Syntax**: The previous `-binding` option has been deprecated and replaced with the new `-b...` 
  option family. Scripts and job submission commands using the old syntax will need to be updated to ensure 
  compatibility with the new framework. Please read the "Notes and Caveats" section in the sge_binding(5) manual page
  we will add examples there that illustrate the transition from legacy to new syntax.
- **Binding with Processor Sets**: The processor set binding implementation has been removed. The new scheduler based 
  binding framework is not available for those architectures (like Solaris). If you need this then please contact our
  support.
- **Configuration Changes**: OCS and GCS are topology aware which eliminates possible hooks and custom scripts that 
  might have been used in the past to make the feature work. Any custom configurations that relied on the legacy binding
  mechanisms should be reviewed and modified to align with the new resource model.
- **Job Scheduling Behavior**: The new binding framework integrates binding requests directly into the scheduling 
  process. This may affect job start times and resource allocation strategies. Administrators should monitor job 
  scheduling behavior after upgrading to ensure that it meets the cluster's performance requirements.
- **Documentation and Training**: Users and administrators should familiarize themselves with the new binding options
  and their implications. Updated documentation and training materials should be provided to facilitate a smooth
  transition.
- **Testing and Validation**: It is recommended to thoroughly test the new binding configurations in a staging 
  environment before deploying them in production. This will help identify any potential issues and ensure that 
  workloads run as expected.
- **Support and Assistance**: For any challenges encountered during the transition, users are encouraged to reach out 
  to the support team for guidance and assistance.

[//]: # (Each file has to end with two empty lines)

