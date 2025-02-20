#pragma once
/*___INFO__MARK_BEGIN_NEW__*/
/***************************************************************************
 *
 *  Copyright 2025 HPC-Gridware GmbH
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 ***************************************************************************/
/*___INFO__MARK_END_NEW__*/

#if defined(OCS_WITH_MUNGE)

#include <munge.h>

#include "uti/sge_dstring.h"

namespace ocs::uti {
   using munge_encode_func_t = munge_err_t (*)(char **cred, munge_ctx_t ctx, const void *buf, int len);
   using munge_decode_func_t = munge_err_t (*)(char *cred, munge_ctx_t ctx, void **buf, int *len, uid_t *uid, gid_t *gid);
   using munge_strerror_func_t = const char *(*)(munge_err_t err);

   using munge_ctx_create_func_t = munge_ctx_t * (*)(void);
   using munge_ctx_copy_func_t = munge_ctx_t * (*)(munge_ctx_t ctx);
   using munge_ctx_destroy_func_t = void (*)(munge_ctx_t ctx);
   using munge_ctx_strerror_func_t = const char * (*)(munge_ctx_t ctx);
   using munge_ctx_get_func_t = munge_err_t (*)(munge_ctx_t ctx, munge_opt_t option, ...);
   using munge_ctx_set_func_t = munge_err_t (*)(munge_ctx_t ctx, munge_opt_t option, ...);

   using munge_enum_is_valid_func_t = int (*)(munge_enum_t type, int val);
   using munge_enum_int_to_str_func_t = const char * (*)(munge_enum_t type, int val);
   using munge_enum_str_to_int_func_t = int (*)(munge_enum_t type, const char *str);

class Munge {
private:
   static void *lib_handle;
   static munge_ctx_create_func_t munge_ctx_create_func;
   static munge_ctx_copy_func_t munge_ctx_copy_func;
   static munge_ctx_destroy_func_t munge_ctx_destroy_func;
   static munge_ctx_strerror_func_t munge_ctx_strerror_func;
   static munge_ctx_get_func_t munge_ctx_get_func;
   static munge_ctx_set_func_t munge_ctx_set_func;
   static munge_enum_is_valid_func_t munge_enum_is_valid_func;
   static munge_enum_int_to_str_func_t munge_enum_int_to_str_func;
   static munge_enum_str_to_int_func_t munge_enum_str_to_int_func;

#if 0
   // Apparently we do not necessarily need a munge context to call munge_encode() and munge_decode().
   // When nullptr is passed as context it will use default options (?).
   // Only if we want to configure specific options, e.g. select a specific cipher, we need a context.
   // As a context may not be shared between threads, we need to create a new context for each thread.
   // Do this by instantiating an instance of Munge having a context as member.
   // @todo for now start with just the static functions and see if we need a context later.
   munge_ctx_t *munge_ctx;
   Munge();
#endif

public:
   static munge_encode_func_t munge_encode_func;
   static munge_decode_func_t munge_decode_func;
   static munge_strerror_func_t munge_strerror_func;

   static bool initialize(dstring *error_dstr);
   static void shutdown();

   static void print_munge_enums();

#if 0
   static Munge *get_instance();
   void print_munge_config();
#endif
};
} // ocs::uti
#endif
