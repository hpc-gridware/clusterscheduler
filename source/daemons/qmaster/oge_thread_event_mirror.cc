/*___INFO__MARK_BEGIN_CLOSED__*/
/*___INFO__MARK_END_CLOSED__*/

#include "uti/sge_component.h"
#include "uti/sge_rmon_macros.h"

#include "sge_thread_main.h"
#include "oge_thread_event_mirror.h"

void
oge_event_mirror_initialize() {
   cl_thread_settings_t *dummy_thread_p = nullptr;
   dstring thread_name = DSTRING_INIT;

   DENTER(TOP_LAYER);
   DPRINTF(("event mirror functionality has been initialized\n"));
   sge_dstring_sprintf(&thread_name, "%s%03d", threadnames[EVENT_MIRROR_THREAD], 0);
   cl_thread_list_setup(&(Main_Control.event_mirror_thread_pool), "event mirror thread pool");
   cl_thread_list_create_thread(Main_Control.event_mirror_thread_pool, &dummy_thread_p, cl_com_get_log_list(),
                                sge_dstring_get_string(&thread_name), 0, oge_event_mirror_main, nullptr, nullptr,
                                CL_TT_EVENT_MIRROR);
   sge_dstring_free(&thread_name);
   DRETURN_VOID;
}

void
oge_event_mirror_terminate() {
   DENTER(TOP_LAYER);

   cl_thread_settings_t *thread = cl_thread_list_get_first_thread(Main_Control.event_mirror_thread_pool);
   while (thread != nullptr) {
      DPRINTF((SFN" gets canceled\n", thread->thread_name));
      cl_thread_list_delete_thread(Main_Control.event_mirror_thread_pool, thread);
      thread = cl_thread_list_get_first_thread(Main_Control.event_mirror_thread_pool);
   }
   DPRINTF(("all "SFN" threads terminated\n", threadnames[EVENT_MIRROR_THREAD]));

   DRETURN_VOID;
}

void *
oge_event_mirror_main(void *arg) {
   DENTER(TOP_LAYER);
   // Don't add cleanup code here. It will never be executed. Instead, register a cleanup function with
   // pthread_cleanup_push()/pthread_cleanup_pop() before and after the call of cl_thread_func_testcancel()

   DRETURN(nullptr);
}
