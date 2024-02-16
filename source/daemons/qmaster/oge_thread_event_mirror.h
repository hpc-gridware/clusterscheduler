#pragma once
/*___INFO__MARK_BEGIN_CLOSED__*/
/*___INFO__MARK_END_CLOSED__*/

struct master_event_mirror_class_t {
   pthread_mutex_t mutex;  ///< mutex to gard all members of this structure
   bool is_running;        ///< is the scheduler thread running
   int thread_id;          ///< next thread id to be used when scheduler is restarted
   bool use_bootstrap;     ///< use bootstrap info to identify if scheduler should be started (true) or ignore that information (false)
};

void
oge_event_mirror_initialize();

void
oge_event_mirror_terminate();

void *
oge_event_mirror_main(void *arg);
