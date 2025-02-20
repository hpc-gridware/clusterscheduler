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
#include <iostream>
#include <dlfcn.h>

#include "sge_log.h"
#include "sge_rmon_macros.h"

#include "ocs_Munge.h"

namespace ocs::uti {
   void *Munge::lib_handle = nullptr;
   munge_encode_func_t Munge::munge_encode_func = nullptr;
   munge_decode_func_t Munge::munge_decode_func = nullptr;
   munge_strerror_func_t Munge::munge_strerror_func = nullptr;
   munge_ctx_create_func_t Munge::munge_ctx_create_func = nullptr;
   munge_ctx_copy_func_t Munge::munge_ctx_copy_func = nullptr;
   munge_ctx_destroy_func_t Munge::munge_ctx_destroy_func = nullptr;
   munge_ctx_strerror_func_t Munge::munge_ctx_strerror_func = nullptr;
   munge_ctx_get_func_t Munge::munge_ctx_get_func = nullptr;
   munge_ctx_set_func_t Munge::munge_ctx_set_func = nullptr;
   munge_enum_is_valid_func_t Munge::munge_enum_is_valid_func = nullptr;
   munge_enum_int_to_str_func_t Munge::munge_enum_int_to_str_func = nullptr;
   munge_enum_str_to_int_func_t Munge::munge_enum_str_to_int_func = nullptr;

   bool Munge::initialize(dstring *error_dstr) {
      DENTER(TOP_LAYER);

      bool ret = true;

      if (lib_handle != nullptr) {
         sge_dstring_sprintf(error_dstr, SFNMAX, MSG_MUNGE_ALREADY_INITIALIZED);
         ret = false;
      }
      if (ret) {
         const char *libmunge = "libmunge.so.2";
         lib_handle = dlopen(libmunge, RTLD_LAZY);
         if (lib_handle == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_OPEN_LIBMUNGE_SS, libmunge, dlerror());
            ret = false;
         }
      }

      const char *func;
      if (ret) {
         func = "munge_encode";
         munge_encode_func = reinterpret_cast<munge_encode_func_t>(dlsym(lib_handle, func));
         if (munge_encode_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_decode";
         munge_decode_func = reinterpret_cast<munge_decode_func_t>(dlsym(lib_handle, func));
         if (munge_decode_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_strerror";
         munge_strerror_func = reinterpret_cast<munge_strerror_func_t>(dlsym(lib_handle, func));
         if (munge_encode_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_ctx_create";
         munge_ctx_create_func = reinterpret_cast<munge_ctx_create_func_t>(dlsym(lib_handle, func));
         if (munge_ctx_create_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_ctx_copy";
         munge_ctx_copy_func = reinterpret_cast<munge_ctx_copy_func_t>(dlsym(lib_handle, func));
         if (munge_ctx_copy_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_ctx_destroy";
         munge_ctx_destroy_func = reinterpret_cast<munge_ctx_destroy_func_t>(dlsym(lib_handle, func));
         if (munge_ctx_destroy_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_ctx_strerror";
         munge_ctx_strerror_func = reinterpret_cast<munge_ctx_strerror_func_t>(dlsym(lib_handle, func));
         if (munge_ctx_strerror_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_ctx_get";
         munge_ctx_get_func = reinterpret_cast<munge_ctx_get_func_t>(dlsym(lib_handle, func));
         if (munge_ctx_get_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_ctx_set";
         munge_ctx_set_func = reinterpret_cast<munge_ctx_set_func_t>(dlsym(lib_handle, func));
         if (munge_ctx_set_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_enum_is_valid";
         munge_enum_is_valid_func = reinterpret_cast<munge_enum_is_valid_func_t>(dlsym(lib_handle, func));
         if (munge_enum_is_valid_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_enum_int_to_str";
         munge_enum_int_to_str_func = reinterpret_cast<munge_enum_int_to_str_func_t>(dlsym(lib_handle, func));
         if (munge_enum_int_to_str_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }
      if (ret) {
         func = "munge_enum_str_to_int";
         munge_enum_str_to_int_func = reinterpret_cast<munge_enum_str_to_int_func_t>(dlsym(lib_handle, func));
         if (munge_enum_str_to_int_func == nullptr) {
            sge_dstring_sprintf(error_dstr, MSG_MUNGE_LOAD_FUNC_SS, func, dlerror());
            ret = false;
         }
      }

      // if some initialization failed, then make sure that we clean up the steps that worked
      if (!ret) {
         shutdown();
      }

      DRETURN(ret);
   }

   void Munge::shutdown() {
      DENTER(TOP_LAYER);

      if (lib_handle != nullptr) {
         dlclose(lib_handle);
         lib_handle = nullptr;
         munge_encode_func = nullptr;
         munge_decode_func = nullptr;
         munge_strerror_func = nullptr;
         munge_ctx_create_func = nullptr;
         munge_ctx_copy_func = nullptr;
         munge_ctx_destroy_func = nullptr;
         munge_ctx_strerror_func = nullptr;
         munge_ctx_get_func = nullptr;
         munge_ctx_set_func = nullptr;
         munge_enum_is_valid_func = nullptr;
         munge_enum_int_to_str_func = nullptr;
         munge_enum_str_to_int_func = nullptr;
      }

      DRETURN_VOID;
   }

   void Munge::print_munge_enums() {
      std::pair<munge_enum_t, std::string> types[]{
         {MUNGE_ENUM_CIPHER, "MUNGE_ENUM_CIPHER"},
         {MUNGE_ENUM_MAC, "MUNGE_ENUM_MAC"},
         {MUNGE_ENUM_ZIP, "MUNGE_ENUM_ZIP"}};

      for (auto type : types) {
         std::cout << "type: " << type.second << std::endl;

         const char *enum_str;
         for (int i = 0; (enum_str = munge_enum_int_to_str_func(type.first, i)) != nullptr; i++) {
             if (munge_enum_is_valid_func(type.first, i)) {
                std::cerr << "  " << i << " = " << enum_str << std::endl;
             }
         }
      }

   }

#if 0
   Munge *Munge::get_instance() {
      Munge *ret{nullptr};

      if (lib_handle != nullptr) {
         ret = new Munge();
      }

      return ret;
   }

   Munge::Munge() {
      munge_ctx = munge_ctx_create_func();
   }

   void Munge::print_munge_config() {
   }
#endif

} // ocs
#endif
