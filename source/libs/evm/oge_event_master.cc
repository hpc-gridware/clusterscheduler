/*___INFO__MARK_BEGIN_NEW__*/
/*___INFO__MARK_END_NEW__*/

#include <pthread.h>

#include "oge_event_master.h"

u_long64
oge_get_next_unique_event_id() {
   static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
   static u_long64 id = 0LL;
   u_long64 ret;

   pthread_mutex_lock(&mutex);
   ret = id++;
   pthread_mutex_unlock(&mutex);
   return ret;
}

