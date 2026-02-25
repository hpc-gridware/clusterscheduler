---
title: sge_reporting
section: 5
header: Reference Manual
footer: __RELEASE__
date: __DATE__
---

# NAME

xxqs_name_sxx_reporting - xxQS_NAMExx reporting file format

# DESCRIPTION

A xxQS_NAMExx system writes a reporting file to $SGE_ROOT/default/common/reporting. The reporting file contains data
that can be used for accounting, monitoring and analysis purposes. It contains information about the cluster (hosts, 
queues, load values, consumables, etc.), about the jobs running in the cluster and about sharetree configuration 
and usage. All information is time related, events are dumped to the reporting file in a configurable interval. It
allows monitoring the "real time" status of the cluster as well as historical analysis.

Writing of the reporting file is by default disabled.
Enabling it, controlling the interval in which it is written and the type of information written to the reporting 
file can be done by modifying the `reporting_params` section of the global configuration, see xxqs_name_sxx_conf(5).

# FORMAT

Two possible formats are supported:

* A new one-line JSON format (JSONL) which allows for easy extensibility and more flexible processing of the accounting data.
* The old colon separated format known from SGE (deprecated).

## One Line JSON Format (JSONL)

Beginning with xxQS_NAMExx 9.0.0 the reporting file by default is written in a one-line
JSON format (JSONL).

The file path is `$SGE_ROOT/$SGE_CELL/common/reporting.jsonl`.

Each line in the file contains a single JSON object which represents a single reporting record.

## Colon Separated Format

For compatibility reasons, the reporting file can be written in the colon-separated format. This format is still
supported but is deprecated and will be removed in a future release.

To enable the colon separated format add `old_reporting=true` to the `reporting_params` in the global
configuration.

The file path is `$SGE_ROOT/$SGE_CELL/common/reporting`.

The accounting file is a text file.  
Each line in the file represents a reporting record.  
Accounting record entries are separated by colon (':') signs.

# RECORD TYPES AND FIELDS

The reporting file contains records of different types.
Each record type has a specific record structure.

The first two fields are common to all reporting records:

* time  
  Time (GMT unix timestamp, JSONL: in microseconds) when the record was created.

* type  
  Type of the reporting record. The different types of records and their structure are described in the following text.

## new_job

The new_job record is written whenever a new job enters the system (usually by a submitting command). It has the 
following fields:

* submission_time  
  Time (GMT unix time stamp, JSONL: in microseconds) when the job was submitted.

* job_number  
  The job number.

* task_number  
  The array task id. Always has the value -1 for new_job records (as we don't have array tasks yet).

* pe_taskid  
  The task id of parallel tasks. Always has the value "none" for new_job records.

* job_name  
  The job name (from -N submission option)

* owner  
  The job owner.

* group  
  The unix group of the job owner.

* project  
  The project the job is running in.

* department  
  The department the job owner is in.

* account  
  The account string specified for the job (from -A submission option).

* priority  
  The job priority (from -p submission option).

## job_log

The job_log record is written whenever a job, an array task, or a pe task is changing status. A status change 
can be the transition from pending to running, but can also be triggered by user actions like suspension of a job. 
It has the following fields:

* event_time  
  Time (GMT unix time stamp, JSONL: in microseconds) when the event was generated.

* event  
  A one word description of the event.

* job_number  
  The job number.

* task_number  
  The array task id. Always has the value -1 for new_job records (as we don't have array tasks yet).

* pe_taskid  
  The task id of parallel tasks. Always has the value "none" for new_job records.

* state  
  The state of the job after the event was processed.

* user  
  The user who initiated the event (or special usernames "qmaster", "scheduler" and "execd" for actions of the 
  system itself like scheduling jobs, executing jobs, etc.).

* host  
  The host from which the action was initiated (e.g., the submit host, the qmaster host, etc.).

* state_time  
  Reserved field for later use.

* submission_time  
  Time (GMT unix time stamp, JSONL: in microseconds) when the job was submitted.

* job_name  
  The job name (from -N submission option)

* owner  
  The job owner.

* group  
  The unix group of the job owner.

* project  
  The project the job is running in.

* department  
  The department the job owner is in.

* account  
  The account string specified for the job (from -A submission option).

* priority  
  The job priority (from -p submission option).

* message  
  A message describing the reported action.

## acct

Records of type acct are accounting records. Normally, they are written whenever a job, a task of an array job, 
or the task of a parallel job terminates. However, for long-running jobs an intermediate acct record is created 
once a day after a midnight. This results in multiple accounting records for a particular job and allows for a 
fine-grained resource usage monitoring over time.

For the contents and structure of the accounting records see xxqs_name_sxx_accounting(5).

## queue

Records of type queue contain state information for queues (queue instances). A queue record has the following fields:

* qname  
  The cluster queue name.

* hostname  
  The hostname of a specific queue instance.

* report_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when a state change was triggered.

* state  
  The new queue state.

## queue_consumable

A queue_consumable record contains information about queue consumable values in addition to queue state information:

* qname  
  The cluster queue name.

* hostname  
  The hostname of a specific queue instance.

* report_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when a state change was triggered.

* state  
  The new queue state.

* consumables  
  Description of consumable values.
  * In the JSONL format consumables is a JSON object with the following structure having an attribute for each consumable, which in turn has two attributes: utilization and capacity.
  * In the old colon separated format consumables is a string with multiple consumable values separated by space. A consumable  
        description has the format `<name>=<actual_value>=<configured value>`.

## host

A host record contains information about hosts and host load values. It contains the following information:

* hostname  
The name of the host.

* report_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when the reported information was generated.

* state  
  The new host state. Currently, xxQS_NAMExx doesn't track a host state, the field is reserved for 
  future use. Always contains the value X.

* load_values  
  * In the JSONL format load_values is a JSON object with the following structure having an attribute for each load value, with the load value name as attribute name and the load value as double value.
  * In the old colon separated format load_values is a string. Information about multiple load values is separated by space. A load value 
    description has the format `<name>=<actual_value>`.

## host_consumable

A host_consumable record contains information about hosts and host consumables. Host consumables can for example 
be licenses. It contains the following information:

* hostname  
  The name of the host.

* report_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when the reported information was generated.

* state  
  The new host state. Currently, xxQS_NAMExx doesn't track a host state, the field is reserved for future use. 
  Always contains the value X.

* consumables  
  Description of consumable values.
  * In the JSONL format consumables is a JSON object with the following structure having an attribute for each consumable, which in turn has two attributes: utilization and capacity.
  * In the old colon separated format consumables is a string with multiple consumable values separated by space. A consumable  
    description has the format `<name>=<actual_value>=<configured value>`.

## sharelog

The xxQS_NAMExx qmaster can dump information about sharetree configuration and use to the reporting file. 
The parameter *sharelog* sets an interval in which sharetree information will be dumped. It is set in the format 
HH:MM:SS. A value of 00:00:00 configures qmaster not to dump sharetree information. Intervals of several minutes 
up to hours are sensible values for this parameter. The record contains the following fields

* current time  
  The present time

* usage time  
  The time used so far

* node name  
  The node name

* user name  
  The user name

* project name  
  The project name

* shares  
  The total shares

* job count  
  The job count

* level  
  The percentage of shares used

* total  
  The adjusted percentage of shares used

* long target share  
  The long target percentage of resource shares used

* short target share  
  The short target percentage of resource shares used

* actual share  
  The actual percentage of resource shares used

* usage  
  The combined shares used

* cpu  
  The cpu used

* mem  
  The memory used

* io  
  The IO used

* long target cpu  
  The long target cpu used

* long target mem  
  The long target memory used

* long target io  
  The long target IO used

## new_ar

A new_ar record contains information about advance reservation objects. Entries of this type will be added if an 
advance reservation is created. It contains the following information:

* ar_submission_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when the advance reservation was created.

* ar_number  
  The advance reservation number identifying the reservation.

* ar_owner  
  The owner of the advance reservation.

## ar_attribute

The ar_attribute record is written whenever a new advance reservation was added or the attribute of an existing 
advance reservation has changed. It has the following fields:

* event_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when the event was generated.

* ar_submission_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when the advance reservation was created.

* ar_number  
  The advance reservation number identifying the reservation.

* ar_name  
  Name of the advance reservation.

* ar_account  
  An account string which was specified during the creation of the advance reservation.

* ar_start_time  
  Start time.

* ar_end_time  
  End time.

* ar_granted_pe  
  The parallel environment which was selected for an advance reservation.

* ar_granted_resources  
  The granted resources which were selected for an advance reservation.

## ar_log

The ar_log record is written whenever a advance reservation is changing status. A status change can be from pending 
to active, but can also be triggered by system events like host outage. It has following fields.

* ar_state_change_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when the event occurred which caused a state change.

* ar_submission_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when the advance reservation was created.

* ar_number  
  The advance reservation number identifying the reservation.

* ar_state  
  The new state.

* ar_event  
  An event id identifying the event which caused the state change.

* ar_message  
  A message describing the event which caused the state change.

## ar_acct

The ar_acct records are accounting records which are written for every queue instance whenever an advance 
reservation terminates. Advance reservation accounting records comprise following fields.

* ar_termination_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when the advance reservation terminated.

* ar_submission_time  
  The time (GMT unix time stamp, JSONL: in microseconds) when the advance reservation was created.

* ar_number  
  The advance reservation number identifying the reservation.

* ar_qname  
  Cluster queue name which the advance reservation reserved.

* ar_hostname  
  The name of the execution host.

* ar_slots  
  The number of slots which were reserved.

# SEE ALSO

xxqs_name_sxx_intro(1), xxqs_name_sxx_accounting(5), sge_conf(5). xxqs_name_sxx_host_conf(5), xxqs_name_sxx_queue_conf(5).

# COPYRIGHT

See xxqs_name_sxx_intro(1) for a full statement of rights and permissions.
