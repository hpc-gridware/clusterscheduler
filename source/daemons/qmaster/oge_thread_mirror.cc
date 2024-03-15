/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include "uti/sge_rmon_macros.h"

#include "oge_thread_mirror.h"
#include "oge_MirrorReaderDataStore.h"
#include "oge_MirrorListenerDataStore.h"
#include "sge_thread_main.h"

static void *
oge_event_mirror_main(void *arg) {
   auto *mirror_thread = static_cast<oge::MirrorDataStore *>(arg);

   return mirror_thread->main(arg);
}

void
oge::event_mirror_initialize() {
   DENTER(TOP_LAYER);
   oge::MirrorDataStore *mirror_thread;

   // create reader mirror
   mirror_thread = new oge::MirrorReaderDataStore();
   Main_Control.mirror_thread_pool.push_back(mirror_thread);
   pthread_create(&mirror_thread->thread, nullptr, oge_event_mirror_main, mirror_thread);
   DPRINTF("added event mirror thread for data store %d\n", mirror_thread->data_store_id);

   // create listener mirror
   mirror_thread = new oge::MirrorListenerDataStore();
   Main_Control.mirror_thread_pool.push_back(mirror_thread);
   pthread_create(&mirror_thread->thread, nullptr, oge_event_mirror_main, mirror_thread);
   DPRINTF("added event mirror thread for data store %d\n", mirror_thread->data_store_id);

   DRETURN_VOID;
}

void
oge::event_mirror_terminate() {
   DENTER(TOP_LAYER);

   // trigger cancel and wakeup so that all threads can reach the cancellation point
   for (auto mirror_thread : Main_Control.mirror_thread_pool) {
      DPRINTF("triggered shutdown/wake up of mirror thread for data store %d\n", mirror_thread->data_store_id);

      pthread_cancel(mirror_thread->thread);
      mirror_thread->wakeup();
   }

   // wait till each thread returns from its main
   for (auto mirror_thread : Main_Control.mirror_thread_pool) {
      pthread_join(mirror_thread->thread, nullptr);
      delete mirror_thread;
      DPRINTF("termination of thread for data store %d finished\n", mirror_thread->data_store_id);
   }

   // empty the container
   Main_Control.mirror_thread_pool.clear();

   DRETURN_VOID;
}
