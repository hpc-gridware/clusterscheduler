### The Scheduler Thread

High level overview.

#### Terminology

resource diagram
: A datastructure storing resource consumption over time. It contains resource (un)availability due to
running jobs consuming resources, resources being reserved by advance reservations and resource reservations, as well as
queue calendars blocking resources.

category cache
: A datastructure caching per category information, e.g. skip lists containing hosts or queue instances
which are not suited for a given category.

#### Environment variables influencing scheduler thread

SGE_PRINT_RESOURCE_UTILIZATION
: When set then at the beginning of every scheduling run
([Function `dispatch_jobs()`](#dispatch-jobs)) the resource diagram is printed with `DPRINTF` calls. 

#### Tagging of queues and resource requests

QU_tag

In Scheduler:

|   Value | Meaning              |
|--------:|----------------------|
|       0 | unsuited for the job |
|    >= 1 |suited for the job and can provide this number of slots  |

Also used in other places to tag queues for selection, e.g. in clients.

QU_tagged4schedule

| Value                  | Meaning                                                         |
|------------------------|-----------------------------------------------------------------|
| TAG4SCHED_NONE         | can not be used                                                 |
| TAG4SCHED_MASTER       | can be used now for the master task of a parallel job           |
| TAG4SCHED_SLAVE        | can be used now as slave queue                                  |
| TAG4SCHED_ALL          | can be used in general                                          |

Also used when after dispatching a job queue instances which is in load alarm by a consumable as load threshold.

#### Scheduler Main

Function `sge_scheduler_main()` in `source/daemons/qmaster/sge_thread_scheduler.cc`

* thread main function
* in an endless loop
   * registers as event client + mirror client and does all the client and event handling
   * preparation of the scheduling run, e.g. update or re-create categories
   * creates copies of master lists which will be modified during a scheduling run
   * starts the [`scheduler_method()`](#scheduler-method)
   * cleans up

#### Scheduler Method

Function `scheduler_method()` in `source/daemons/qmaster/sge_sched_thread.cc`

* initializations
* splits job list into multiple lists according to the job state (pending, waiting, suspended, ...),
  the pending jobs are the ones we are interested in
* generates global scheduler messages (for queues which are not available for scheduling)
* does the actual scheduling [`dispatch_jobs()`](#dispatch-jobs)
* sends orders to worker threads (a part of the job start orders might already have been sent during dispatching)
* cleans up


#### Dispatch Jobs

Function `dispatch_jobs()`.

* preparing the actual dispatching
  * correction of virtual load (see `job_load_adjustments` in sge_sched_conf(5))
  * update the resource diagram (running/suspended jobs, queue calendars)
  * correction of capacities (configured host consumable capacities will be reduced by external usage - from load values)
  * handle overloaded queues
    * detect overloaded queues
    * unsuspend jobs which were suspended by suspend threshold but threshold is no longer exceeded
    * suspend jobs in queues which are overloaded according to suspend threshold
    * these actions generate scheduler orders, the actual suspension/unsuspension will then be done by worker threads
  * remove queues which are overloaded due to load threshold
  * remove calendar disabled queues
  * remove queues which are full (no free slots)
  * remove jobs which would exceed the per user running job limit
  * calculate ticket counts and priorities, see [SGEEE scheduler](#sgeee-scheduler)
  * sort jobs by priority
  * sort the host list
* loop over the pending jobs
  * try to schedule them, see [Select Assign Debit](#select-assign-debit).
  * if a job could be scheduled
    * generate orders
    * re-evaluate per user job limits
    * if necessary re-sort the pending job list


#### SGEEE Scheduler

#### Select Assign Debit

Function `select_assign_debit()`.

Schedules a single job / array task = 
*Selects* resources, *Assignes* them to the job, *Debits* the resources from the resource booking.

Preparation:
* initialize a `sge_assignment_t` structure

Scheduling:
* calls [Select Parallel Environment](#select-parallel-environment) for parallel jobs, once for immediate scheduling, 
  optionally (when the job couldn't be scheduled but requested resource reservation) a second time to do
  a resource reservation
* calls []() for sequential jobs, once for immediate scheduling,
  optionally (when the job couldn't be scheduled but requested resource reservation) a second time to do
  a resource reservation

Post processing:
* create orders
* debit the job from resources [Debit Scheduled Job](#debit-scheduled-job)
* sort out queues which are now no longer available due to no more slots, alarm or suspended state.
* free the `sge_assignment_t` structure

#### Select Parallel Environment

Function `sge_select_parallel_environment()`.

* sort queues according to host order
* initialize queue tags

In case we are doing a reservation (AR or RR)
* loop over all PEs matching the PE request
  * find the earliest start time with this PE: [`parallel_reservation_max_time_slots()`](#parallel-reservation-max-time-slots)
  * if there are multiple PEs matching the request try to find "the best" result

In case of scheduling "now":
* in case job shall run in an AR
  * we just have a single PE (the one granted to the AR)
  * try to find an assignment for the job: [`parallel_maximize_slots_pe()`](#parallel-maximize-slots-pe)
* without AR
  * loop over all PEs matching the PE request
    * try to find an assignment: [`parallel_maximize_slots_pe()`](#parallel-maximize-slots-pe)
    * if there are multiple PEs matching the request try to find "the best" result

* evaluate the result
  * in case of reservations: try to maximize the number of slots: [`parallel_maximize_slots_pe()`](#parallel-maximize-slots-pe)

* cleanup

#### Parallel Reservation Max Time Slots

Function `parallel_reservation_max_time_slots()`.

Search the earliest assignment (reservation) for a job and a specific PE.

* initialize category cache
* use configured algorithm to maximize the slot count
  * binary search
  * from minimum to maximum
  * from maximum to minimum
  * in case a fixed slot count the algorithm will be passed only once
* for every possible slot count
  * call function [`parallel_assignment()`](#parallel-assignment)
  * evaluate the result, if it gives us a possible assignment, remember it
  * the code stops the search if the maximum possible slot count is reached

#### Parallel Maximize Slots PE

Function `parallel_maximize_slots_pe()`.

Search for the maximum number of slots (in case of pe ranges) for the job in a specific PE.

* initialize category cache

#### Parallel Assignment

Function `parallel_assignment()`.

Returns (if possible) an assignment for a particular PE with a fixed slot count at a fixed time.

* check if the requested amount of slots is available in the PE in the first place: `parallel_available_slots()`
* call [`parallel_tag_queues_suitable4job()`](#parallel-tag-queues-suitable4job)

#### Parallel Tag Queues Suitable4job

Function `parallel_tag_queues_suitable4job()`.

* untag all queue instances (value 0 = 0 slots available = unsuited)
* check global resources: `parallel_global_slots()`
* first queue filtering step
  * loop over the queue list
    * static cluster queue check: `cqueue_match_static()`
* queue sorting step
  * if we have soft requests: calculate the soft violations: `get_soft_violations()`
  * sort the queue list
* sort the host list
* loop over the sorted host list
  * for every possible host: call [`parallel_tag_hosts_queues()`](#parallel-tag-hosts-queues).
    It gives us the number of slot we can get from this host as well as if it is suitable as master host
  * if we want to use the host, check if RQS allows using it: `parallel_check_and_debit_rqs_slots()`
  * if RQS does not allow to run the job, then undebit from RQS: `parallel_revert_rqs_slot_debitation()`
  * if we have gathered both the master task as well as all requested slave tasks then break out of the loop

#### Parallel Tag Hosts Queues

Function `parallel_tag_hosts_queues()`.

* check how many slots the host can give us
  * from an AR
  * from host (and its queues): [`parallel_host_slots()`](#parallel-host-slots)


#### Parallel Host Slots

Function `parallel_host_slots()`.

Determines how many slots are available for a job at a given time.
Tags available queue instances.

* clears resource tags for queues and hosts
* checks if queue instances are rejected by -q or -masterq requests
* does host static matching: `sge_host_match_static()`
* [`parallel_rc_slots_by_time()`](#parallel-rc-slots-by-time)
* if slots are available, maximize their number: `parallel_max_host_slots()`
* from the calculated slots subtract the already booked slots (from the GDIL, which we build up during scheduling).

#### Parallel RC Slots By Time

Determine the maximum number of slots a host can deliver
as limited by the job requests (*and implicit and default requests*).

* clears resource tags for queues
* check implicit slots request (if slots is defined on host level)
* check default requests of complexes
* check explicit requests
* evaluates the results of the individual checks and calculates the combined available host slots

Checking of requests is done by calling [`ri_slots_by_time`](#ri-slots-by-time)



#### RI Slots By Time

Calculates the amount of slots which can be assigned according to an individual resource request
for a specific resource (host or queue instance).
Either for now or for a specific time and for in general (queue end).

#### Sequential Assignment

Function `sge_sequential_assignment()`.

* if necessary re-sort hosts and queue instances
* call [`sequential_tag_queues_suitable4job()`](#sequential-tag-queues-suitable4job) to select
  a queue instance where the job can run
* evaluate the scheduling result, in case of success prepare debiting and job start
  (by creating the JAT_granted_destin_identifier_list)

#### Sequential Tag Queues Suitable4job

Function `sequential_tag_queues_suitable4job`

* initialise scheduler messages and skip lists from the category cache
* check if the job can run in its AR (if any), static matching: `match_static_advance_reservation()`
* check if it requests global resources and there is enough free: `sequential_global_time()`
* loop over the queue list and check if the queue is suitable for the job
  * skip queues / hosts which are in skip lists
  * check resource quota (unless the job is submitted into an AR): `rqs_by_slots()`
  * static matching on cluster queue layer: `cqueue_match_static()`
  * check if the job requested a host other than the qinstance host: `qref_list_eh_rejected()`
  * static host matching: `sge_host_match_static()`
  * static queue matching: `sge_queue_match_static()`
  * AR matching (if any)
  * non AR:
    * dynamic host matching: `sequential_host_time()`
    * dynamic queue matching: `sequential_queue_time()`
  * evaluate the result, if it is "the best we can achieve" then break the loop
* cleanup
* store scheduler messages to the category cache

> @todo queue tagging could probably be optimized:
> * in case of now scheduling it should not be necessary at all, just return the first found queue somehow
> * in case of reservation scheduling we could also return the best result and just take it instead of re-iterating
>   over the queue list


#### Debit Scheduled Job

Function `debit_scheduled_job()`


#### Configuration for performance optimization

In many clusters, especial when there is no external load,
we can switch off a few potentially expensive features and just rely on scheduling by slots:
* do not configure queue load_thresholds and suspend_thresholds
* do not use load adjustments (in the scheduler config)

[//]: # (Eeach file has to end with two emty lines)

