/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include <cstdlib>
#include <cstdio>
#include <pthread.h>

#include "uti/sge_bootstrap.h"
#include "uti/sge_dstring.h"
#include "uti/sge_fgl.h"
#include "uti/sge_rmon_macros.h"

#include "sgeobj/cull/sge_all_listsL.h"
#include "sgeobj/sge_cqueue.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_object.h"

#define THREADS 32L
#define REQUESTS (1024L*1024L*16L)

#define JOBS 256
#define CQUEUES 32 
#define HOSTS 1024 
#define PROJECTS 256

#define SCENARIOS 10

typedef struct {
   long id;
} thread_arg_t;

static pthread_t threads[THREADS];
static thread_arg_t thread_args[THREADS];

static pthread_mutex_t request_mtx = PTHREAD_MUTEX_INITIALIZER;
static int requests[REQUESTS];
static long request = 0;

#define _DENTER(layer, function) \
   const char *__func__ = function;                                  \
   const int xaybzc = layer;                                          \
                                                                             \
   if (rmon_condition(xaybzc, TRACE)) {                                      \
      cl_thread_settings_t* ___thread_config = cl_thread_get_thread_config();\
      if (___thread_config != nullptr) {                                        \
         rmon_menter (__func__, ___thread_config->thread_name);              \
      } else {                                                               \
         rmon_menter (__func__, nullptr);                                       \
      }                                                                      \
   } 

void simulate_job_add(void) {
   DENTER(TOP_LAYER);
   fgl_lock();

#if 0
   dstring dump = DSTRING_INIT;
   fgl_dump(&dump);
   fprintf(stderr, sge_dstring_get_string(&dump));
   sge_dstring_free(&dump);
#endif

   // sleep(1);
   fgl_unlock();
   DRETURN_VOID;
}

void simulate_job_start(u_long32 jid, const char *cqueue, const char *host) {
   DENTER(TOP_LAYER);
   fgl_lock();

#if 0
   dstring dump = DSTRING_INIT;
   fgl_dump(&dump);
   fprintf(stderr, sge_dstring_get_string(&dump));
   sge_dstring_free(&dump);
#endif

   // sleep(1);
   fgl_unlock();
   DRETURN_VOID;
}

void simulate_load_report(const char *host) {
   DENTER(TOP_LAYER);
   fgl_lock();

#if 0
   dstring dump = DSTRING_INIT;
   fgl_dump(&dump);
   fprintf(stderr, sge_dstring_get_string(&dump));
   sge_dstring_free(&dump);
#endif

   // sleep(1);
   fgl_unlock();
   DRETURN_VOID;
}

void simulate_project_update(const char *host) {
   DENTER(TOP_LAYER);
   fgl_lock();

#if 0
   dstring dump = DSTRING_INIT;
   fgl_dump(&dump);
   fprintf(stderr, sge_dstring_get_string(&dump));
   sge_dstring_free(&dump);
#endif

   // sleep(1);
   fgl_unlock();
   DRETURN_VOID;
}


void *thread1(void *data) {
   thread_arg_t *thread = (thread_arg_t *)data;
   dstring thread_name = DSTRING_INIT;

   sge_dstring_sprintf(&thread_name, "thread%d", thread->id);
   _DENTER(TOP_LAYER, sge_dstring_get_string(&thread_name));
   //sge_dstring_free(&thread_name);

   lInit(nmv); 

   while (true) {
      pthread_mutex_lock(&request_mtx);
      if (request > REQUESTS) {
         pthread_mutex_unlock(&request_mtx);
         DRETURN(nullptr);
      } 
      long request_id = requests[request];
      request++;
      pthread_mutex_unlock(&request_mtx);

      switch (request_id) {
         case 0:
            // job add
            {
               dstring cqueue_name = DSTRING_INIT;
               dstring host_name = DSTRING_INIT;

               u_long32 jid = rand() % JOBS;
               sge_dstring_sprintf(&cqueue_name, "cqueue%d", rand() % CQUEUES);
               sge_dstring_sprintf(&host_name, "host%d", rand() % HOSTS);

               fgl_clear();
               fgl_add_r(SGE_TYPE_JOB, false);
               fgl_add_u(SGE_TYPE_JOB, jid, true);
               fgl_add_r(SGE_TYPE_CQUEUE, false);
               fgl_add_s(SGE_TYPE_CQUEUE, sge_dstring_get_string(&cqueue_name), true);
               fgl_add_r(SGE_TYPE_EXECHOST, false);
               fgl_add_s(SGE_TYPE_EXECHOST, sge_dstring_get_string(&host_name), true);

               simulate_job_add();

               sge_dstring_free(&cqueue_name);
               sge_dstring_free(&host_name);
            }
            break;
         case 1:
            // job start
            {
               dstring cqueue_name = DSTRING_INIT;
               dstring host_name = DSTRING_INIT;

               u_long32 jid = rand() % JOBS;
               sge_dstring_sprintf(&cqueue_name, "cqueue%d", rand() % CQUEUES);
               sge_dstring_sprintf(&host_name, "host%d", rand() % HOSTS);

               fgl_clear();
               fgl_add_r(SGE_TYPE_JOB, false);
               fgl_add_u(SGE_TYPE_JOB, jid, true);
               fgl_add_r(SGE_TYPE_CQUEUE, false);
               fgl_add_s(SGE_TYPE_CQUEUE, sge_dstring_get_string(&cqueue_name), true);
               fgl_add_r(SGE_TYPE_EXECHOST, false);
               fgl_add_s(SGE_TYPE_EXECHOST, sge_dstring_get_string(&host_name), true);

               simulate_job_start(jid, sge_dstring_get_string(&cqueue_name), sge_dstring_get_string(&host_name));

               sge_dstring_free(&cqueue_name);
               sge_dstring_free(&host_name);
            }
            break;
         case 2:
         case 3:
         case 4:
         case 5:
            // load report 
            {
               dstring host_name = DSTRING_INIT;

               sge_dstring_sprintf(&host_name, "host%d", rand() % HOSTS);

               fgl_clear();
               fgl_add_r(SGE_TYPE_EXECHOST, false);
               fgl_add_s(SGE_TYPE_EXECHOST, sge_dstring_get_string(&host_name), true);

               simulate_load_report(sge_dstring_get_string(&host_name));

               sge_dstring_free(&host_name);
            }
            break;
         case 6:
         case 7:
         case 8:
         case 9:
            // project update 
            {
               dstring project_name = DSTRING_INIT;

               sge_dstring_sprintf(&project_name, "prj%d", rand() % HOSTS);

               fgl_clear();
               fgl_add_r(SGE_TYPE_PROJECT, false);
               fgl_add_s(SGE_TYPE_PROJECT, sge_dstring_get_string(&project_name), true);

               simulate_project_update(sge_dstring_get_string(&project_name));

               sge_dstring_free(&project_name);
            }
            break;
      }
   }
   DRETURN(nullptr);
}

int main(int argc, char *argv[]) {
   DENTER_MAIN(TOP_LAYER, "test_fgl");
   bool ret = true;
 
   // init required modules 
   lInit(nmv); 

   // create some pseudo requests
   srand(time(nullptr));
   for (long r = 0; r < REQUESTS; r++) {
      requests[r] = rand() % SCENARIOS;
   }
   request = 0;

   // create some CULL objects that simulation functions can access
   lList **job_list = oge::DataStore::get_master_list_rw(SGE_TYPE_JOB);
   for (int j = 0; j < JOBS; j++) {
      lAddElemUlong(job_list, JB_job_number, j, JB_Type);
   }

   lList **host_list = oge::DataStore::get_master_list_rw(SGE_TYPE_EXECHOST);
   for (int h = 0; h < HOSTS; h++) {
      dstring host_name = DSTRING_INIT;

      sge_dstring_sprintf(&host_name, "host%d", h);
      lAddElemHost(host_list, EH_name, sge_dstring_get_string(&host_name), EH_Type);
      sge_dstring_free(&host_name);
   }

   lList **cqueue_list = oge::DataStore::get_master_list_rw(SGE_TYPE_CQUEUE);
   for (int q = 0; q < CQUEUES; q++) {
      dstring cqueue_name = DSTRING_INIT;

      sge_dstring_sprintf(&cqueue_name, "cqueue%d", q);
      lAddElemStr(cqueue_list, CQ_name, sge_dstring_get_string(&cqueue_name), CQ_Type);
      sge_dstring_free(&cqueue_name);
   }

   lList **project_list = oge::DataStore::get_master_list_rw(SGE_TYPE_PROJECT);
   for (int p = 0; p < PROJECTS; p++) {
      dstring project_name = DSTRING_INIT;

      sge_dstring_sprintf(&project_name, "prj%d", p);
      lAddElemStr(project_list, PR_name, sge_dstring_get_string(&project_name), PR_Type);
      sge_dstring_free(&project_name);
   }

   // END OF SINGLE THREADED INIT

   // spawn threads
   for (long t = 0; t < THREADS; t++) {
      thread_args[t].id = t;
      pthread_create(&threads[t], nullptr, thread1, &thread_args[t]);
   }

   // join threads
   for (long t = 0; t < THREADS; t++) {
      pthread_join(threads[t], nullptr);
   }

   // dump statistics (if enabled)
   dstring stats_str = DSTRING_INIT;
   fgl_dump_stats(&stats_str);
   fprintf(stderr, "%s", sge_dstring_get_string(&stats_str));

   DRETURN(ret ? EXIT_SUCCESS : EXIT_FAILURE);
}

