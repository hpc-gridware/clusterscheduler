/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 *
 *  The Contents of this file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 *
 *  Sun Microsystems Inc., March, 2001
 *
 *
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of this file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use this file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 *
 *  Software provided under this License is provided on an "AS IS" basis,
 *  WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING,
 *  WITHOUT LIMITATION, WARRANTIES THAT THE SOFTWARE IS FREE OF DEFECTS,
 *  MERCHANTABLE, FIT FOR A PARTICULAR PURPOSE, OR NON-INFRINGING.
 *  See the License for the specific provisions governing your rights and
 *  obligations concerning the Software.
 *
 *   The Initial Developer of the Original Code is: Sun Microsystems, Inc.
 *
 *   Copyright: 2001 by Sun Microsystems, Inc.
 *
 *   All Rights Reserved.
 *
 *  Portions of this software are Copyright (c) 2023-2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Localisation: gettext initialisation and message translation
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#include "uti/sge_arch.h"
#include "uti/sge_htable.h"
#include "uti/sge_language.h"
#include "uti/sge_mtutil.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdlib.h"

#include <cinttypes>

#include <sge_log.h>

#ifdef __SGE_COMPILE_WITH_GETTEXT__

/* MT-NOTE: language_mutex guards all language module function calls */
static pthread_mutex_t language_mutex = PTHREAD_MUTEX_INITIALIZER;

#   define LANGUAGE_LOCK()            sge_mutex_lock("language_mutex", __func__, __LINE__, &language_mutex)
#   define LANGUAGE_UNLOCK()          sge_mutex_unlock("language_mutex", __func__, __LINE__, &language_mutex)

/*
 *  environment variable "SGE_ENABLE_MSG_ID"
 *  supported values: 0 - off
 *                    1 - on for error messages
 *                    2 - on for every message
 */
#define SGE_ENABLE_MSG_ID "SGE_ENABLE_MSG_ID"
#define PACKAGE "GRIDPACKAGE"
#define LOCALEDIR "GRIDLOCALEDIR"
#define SGE_DEFAULT_PACKAGE      "clusterscheduler"
#define SGE_DEFAULT_LOCALEDIR    "locale"


/* this is to define the functions for the internationalization
   tools */
typedef struct {
   gettext_func_type gettext_func;
   setlocale_func_type setlocale_func;
   bindtextdomain_func_type bindtextdomain_func;
   textdomain_func_type textdomain_func;
} language_functions_struct;

/** @brief One cached message of the message id hash table; never released, the table lives as long as the process */
typedef struct {
   int id{0};                 ///< message identification number
   int category{0};           ///< message category: 0 = common
   uint32_t counter{0};       ///< number of calls
   std::string message;       ///< message string
   std::string local_message; ///< local translated message, with the id prefix where applicable
} sge_error_message_t;

static pthread_once_t message_id_once = PTHREAD_ONCE_INIT;

static void message_id_once_init();

static pthread_key_t message_id_key;

static void message_id_destroy(void *theState);

static language_functions_struct sge_language_functions;
static int sge_enable_msg_id = 0;    /* used to enable/disable message ids */
static int sge_enable_msg_id_to_every_message = 0;
static htable sge_message_hash_table = nullptr;

/* this is to find out if the sge_init_language_func() was called */
static bool sge_are_language_functions_installed = false;

static int sge_get_message_id_output_implementation();

/**
 * @brief Initialize language package for gettext()
 *
 * starts up the language initialization for gettext(). This
 * function should be called nearly after the main() function.
 *
 * sge_init_language_func() must be called first to install the
 * correct function pointers for gettext() setlocale() etc. etc.
 *
 * @param package package name like "clusterscheduler" of binary package *.mo file; if nullptr,
 *                sge_init_language tries to get the package name from the environment variable "GRIDPACKAGE"
 * @param localeDir path to the localization directory; if nullptr, sge_init_language tries to get the
 *                  localization directory path from the environment variable "GRIDLOCALEDIR"
 *
 * @return true for success, false on error.
 *
 * @note MT-NOTE: sge_init_languagefunc() is guarded by language_mutex
 *
 * @see #sge_init_language, #sge_init_language_func, #sge_gettext, #sge_gettext_, #sge_gettext__,
 *      #sge_get_message_id_output, #sge_set_message_id_output
 */
int sge_init_languagefunc(char *package, char *localeDir) {
   DENTER_(TOP_LAYER);

   std::string packName;
   std::string locDir;
   std::string language;
   std::string pathName;
   char *sge_enable_msg_id_string = nullptr;
   int success = false;
   int back;
   int stop = 0;

   LANGUAGE_LOCK();

   DPRINTF_("****** starting localization procedure ... **********\n");


   while (stop <= 2) {

      /* reset for evt. second run */
      bool have_pack_name = false;
      bool have_loc_dir = false;
      language.clear();

      /* try to get the package name */
      if (package != nullptr) {
         packName = package;
         have_pack_name = true;
      } else if (getenv(PACKAGE) != nullptr) {
         packName = getenv(PACKAGE);
         have_pack_name = true;
         DPRINTF_("try to get language package name from environment " SFQ "\n", PACKAGE);
      } else {
         DPRINTF_("could not get environment variable " SFQ "\n", PACKAGE);
      }

      /* no package name given, using default one */
      if (!have_pack_name) {
         packName = std::string(sge_get_arch()) + "/" + SGE_DEFAULT_PACKAGE;
      }

      /* try to get the localization directory */
      if (localeDir != nullptr) {
         locDir = localeDir;
         have_loc_dir = true;
      } else if (getenv(LOCALEDIR) != nullptr) {
         if (stop != 0) {
            DPRINTF_("ignoring environment " SFQ "\n", LOCALEDIR );
         } else {
            locDir = getenv(LOCALEDIR); /* only the first time */
            have_loc_dir = true;
            stop++;
            DPRINTF_("try to get language package directory path from environment " SFQ "\n", LOCALEDIR);
         }
      } else {
         DPRINTF_("could not get environment variable " SFQ "\n", LOCALEDIR);
      }

      /* no directory given, using default one */
      if (!have_loc_dir) {
         const char *sge_root = sge_get_root_dir(0, nullptr, 0, 0);
         if (sge_root == nullptr) {
            locDir = "/usr/lib/locale";
         } else {
            locDir = std::string(sge_root) + "/" + SGE_DEFAULT_LOCALEDIR;
         }
      }

      /* try to get a language stylename (only for output of package path) */
      const char *language_var = getenv("LANGUAGE");
      if (language_var == nullptr) {
         language_var = getenv("LANG");
      }
      if (language_var == nullptr) {
         DPRINTF_("environment LANGUAGE or LANG is not set; no language selected - using defaults\n");
         language_var = "C";
      }

      /* get correct language value */
      if (sge_language_functions.setlocale_func != nullptr) {
         char *help1 = nullptr;
         help1 = sge_language_functions.setlocale_func(LC_MESSAGES, "");
         if (help1 != nullptr) {
            char *slash_pos = nullptr;
            DPRINTF_("setlocale() returns \"%s\"\n", help1);
            slash_pos = strstr(help1, "_");
            if (slash_pos != nullptr) {
               DPRINTF_("cutting of language string after \"_\":\n");
               language.assign(help1, slash_pos - help1);  /* cut off "_" */
            } else {
               language = help1;
            }
         } else {
            DPRINTF_("setlocale() returns nullptr");
            language = language_var;
         }
      }

      /* try to setup the choosen language package (*.mo - file); without a setlocale function the language
         stays empty (that called strlen(nullptr) before) */
      pathName = locDir + "/" + language + "/LC_MESSAGES/" + packName + ".mo";
      DPRINTF_("locale directory: >%s<\n", locDir.c_str());
      DPRINTF_("package file:     >%s.mo<\n", packName.c_str());
      DPRINTF_("language (LANG):  >%s<\n", language.c_str());
      DPRINTF_("loading message file: %s\n", pathName.c_str());

      /* is the package file allright */
      back = open(pathName.c_str(), O_RDONLY);
      if (back >= 0) {
         DPRINTF_("found message file - ok\n");
         success = true;
         close(back);
         stop = 2;
      } else {
         DPRINTF_("could not open message file - error\n");
         success = false;
      }
      if (stop == 0) {
         stop = 2;   /* only when stop = 1: Try to repeat without LOCALEDIR environment variable */
      }
      stop++;
   }


   /* now startup the language package for gettext */
/*   setlocale(LC_ALL, ""); */

   if (sge_language_functions.setlocale_func != nullptr &&
       sge_language_functions.bindtextdomain_func != nullptr &&
       sge_language_functions.textdomain_func != nullptr &&
       sge_are_language_functions_installed) {
      char *help1 = nullptr;
      help1 = sge_language_functions.setlocale_func(LC_MESSAGES, "");
      if (help1 != nullptr) {
         DPRINTF_("setlocale() returns \"%s\"\n", help1);
      } else {
         DPRINTF_("setlocale() returns nullptr\n");
      }
      help1 = sge_language_functions.bindtextdomain_func(packName.c_str(), locDir.c_str());
      if (help1 != nullptr) {
         DPRINTF_("bindtextdomain() returns \"%s\"\n", help1);
      } else {
         DPRINTF_("bindtextdomain() returns nullptr\n");
      }
      help1 = sge_language_functions.textdomain_func(packName.c_str());
      if (help1 != nullptr) {
         DPRINTF_("textdomain() returns \"%s\"\n", help1);
      } else {
         DPRINTF_("textdomain() returns nullptr\n");
      }
   } else {
      DPRINTF_("sge_init_language() called without valid sge_language_functions pointer!\n");
      success = false;
   }

   if (success) {
      sge_enable_msg_id = 1;
   }

   sge_enable_msg_id_string = getenv(SGE_ENABLE_MSG_ID);
   if (sge_enable_msg_id_string != nullptr) {
      int env_value = 0;
      DPRINTF_("SGE_ENABLE_MSG_ID is set to \"%s\"\n", sge_enable_msg_id_string);
      env_value = atoi(sge_enable_msg_id_string);
      if (env_value == 0) {
         sge_enable_msg_id = 0;
      } else {
         sge_enable_msg_id = 1;
      }
      if (env_value == 2) {
         sge_enable_msg_id_to_every_message = 1;
      }
   }


   if (sge_enable_msg_id == 0) {
      DPRINTF_("error id output     : disabled\n");
   } else {
      DPRINTF_("error id output     : enabled\n");
   }

   if (success) {
      DPRINTF_("****** starting localization procedure ... success **\n");
   } else {
      DPRINTF_("****** starting localization procedure ... failed  **\n");
   }

   LANGUAGE_UNLOCK();

   DRETURN_(success);
}

/**
 * @brief Install language functions
 *
 * set the function pointer for the gettext(), setlocale(),
 * bindtextdomain() and textdomain() function calls. This function
 * must called before any call to sge_init_language() and
 * sge_gettext().
 *
 * @param gettext_func_type pointer for gettext()
 * @param setlocale_func_type pointer for setlocale()
 * @param bindtextdomain_func_type pointer for bindtextdomain()
 * @param textdomain_func_type pointer for textdomain()
 *
 * @note MT-NOTE: sge_init_language_func() is guarded by language_mutex
 *
 * @see #sge_init_language, #sge_init_language_func, #sge_gettext, #sge_gettext_, #sge_gettext__, #sge_get_message_id_output, #sge_set_message_id_output
 */
void sge_init_language_func(gettext_func_type new_gettext,
                            setlocale_func_type new_setlocale,
                            bindtextdomain_func_type new_bindtextdomain,
                            textdomain_func_type new_textdomain) {
   DENTER_(TOP_LAYER);

   LANGUAGE_LOCK();

   /* initialize the functions pointer to nullptr */
   sge_language_functions.gettext_func = nullptr;
   sge_language_functions.setlocale_func = nullptr;
   sge_language_functions.bindtextdomain_func = nullptr;
   sge_language_functions.textdomain_func = nullptr;

   /* ok, the functions have now nullptr pointer */
   sge_are_language_functions_installed = true;

   /* set the new functions */
   if (new_gettext != nullptr) {
      sge_language_functions.gettext_func = new_gettext;
   }

   if (new_setlocale != nullptr) {
      sge_language_functions.setlocale_func = new_setlocale;
   }

   if (new_bindtextdomain != nullptr) {
      sge_language_functions.bindtextdomain_func = new_bindtextdomain;
   }

   if (new_textdomain != nullptr) {
      sge_language_functions.textdomain_func = new_textdomain;
   }

   LANGUAGE_UNLOCK();

   DRETURN_VOID_;
}

/**
 * @brief Enable message id number adding
 *
 * This procedure is used to enable the adding of message id's when
 * showing error messages. This function is used in the macro
 * SGE_ADD_MSG_ID(x) to enable the message id for errors.
 *
 * @param flag 0 = off ; 1 = on
 *
 * @note MT-NOTE: sge_set_message_id_output() is guarded by language_mutex
 *
 * @see #sge_init_language, #sge_init_language_func, #sge_gettext, #sge_gettext_, #sge_gettext__, #sge_get_message_id_output, #sge_set_message_id_output
 */
void sge_set_message_id_output(int flag) {
   DENTER_(TOM_LAYER);

   int *buf = nullptr;

   pthread_once(&message_id_once, message_id_once_init);

   buf = (int *) pthread_getspecific(message_id_key);

   if (buf != nullptr) {
      *buf = flag;
   }

   DRETURN_VOID_;
}

/**
 * @brief Check if message id should be added
 *
 * This function returns the value stored in the static global
 * variable sge_message_id_view_flag.
 *
 * @return value of sge_message_id_view_flag
 *
 * @note MT-NOTE: sge_get_message_id_output() is guarded by language_mutex
 *
 * @see #sge_init_language, #sge_init_language_func, #sge_gettext, #sge_gettext_, #sge_gettext__, #sge_get_message_id_output, #sge_set_message_id_output, #sge_get_message_id_output_implementation
 */
int sge_get_message_id_output() {
   DENTER_(TOP_LAYER);

   int ret;

   LANGUAGE_LOCK();
   ret = sge_get_message_id_output_implementation();
   LANGUAGE_UNLOCK();

   DRETURN_(ret);
}

/**
 * @brief Pure implementation of
 *
 * When the sge_get_message_id_output() functionality is needed from within
 * the language modules the language_mutex may not be obtained because this
 * mutex is already owned by the same thread.
 *
 * @return value of sge_message_id_view_flag
 *
 * @note MT-NOTE: The caller of sge_get_message_id_output_implementation() must
 *       MT-NOTE: have obtained the language_mutex mutex due to access to
 *       MT-NOTE: 'sge_enable_msg_id' and 'sge_enable_msg_id_to_every_message'.
 *
 * @see #sge_init_language, #sge_init_language_func, #sge_gettext, #sge_gettext_, #sge_gettext__, #sge_get_message_id_output, #sge_set_message_id_output
 */
static int sge_get_message_id_output_implementation() {
   DENTER_(TOM_LAYER);

   int *buf;
   if (sge_enable_msg_id_to_every_message == 1) {
      DRETURN_(1);
   }

   if (sge_enable_msg_id == 0) {
      DRETURN_(0);
   }

   pthread_once(&message_id_once, message_id_once_init);

   buf = (int *) pthread_getspecific(message_id_key);

   if (buf != nullptr) {
      DRETURN_(0);
   } else {
      DRETURN_(*buf);
   }
}

/**
 * @brief Dummy gettext() function
 *
 * This function returns the given argument
 *
 * @param x string
 *
 * @return input string
 *
 * @see #sge_init_language, #sge_init_language_func, #sge_gettext, #sge_gettext_, #sge_gettext__, #sge_get_message_id_output, #sge_set_message_id_output
 */
const char *sge_gettext(const char *x) {
   return x;
}

/**
 * @brief Add the message id to a translated (gettext) message string.
 *
 * Prepends the message id as "[id] " to the translated message, but only when
 * message-id output is enabled (sge_get_message_id_output() != 0) and @p msg_str
 * contains at least one space. Results are cached in a hash table keyed by the
 * message id.
 *
 * @param[in] msg_id  message id (positive message-catalog id; the "[id] " prefix
 *                    is added for ids <= 99999)
 * @param[in] msg_str message to translate
 * @return the translated (L10N) message, with the id prefix where applicable
 *
 * @see sge_init_language(), sge_init_language_func(), sge_gettext(),
 *      sge_gettext__(), sge_get_message_id_output(), sge_set_message_id_output()
 */
const char *sge_gettext_(int msg_id, const char *msg_str) {
   /* extern */
#ifndef __SGE_COMPILE_WITH_GETTEXT__
   return msg_str;
#else
   DENTER_(TOM_LAYER);

   union {
      sge_error_message_t *l;
      void *p;
   } message_p;
   long key;

   message_p.l = nullptr;

   if (msg_str == nullptr) {
      DRETURN_(nullptr);
   }

   key = msg_id;
   /* check if message is not just one word (strstr) */
   if ((sge_get_message_id_output_implementation() != 0) &&
       (strstr(msg_str, " ") != nullptr)) {

      if (sge_message_hash_table == nullptr) {
         sge_message_hash_table = sge_htable_create(8,   /* 2^8 = 256 table start size */
                                                    dup_func_long,
                                                    hash_func_long,
                                                    hash_compare_long);
      }
      if (sge_htable_lookup(sge_message_hash_table, &key, (const void **) &message_p.p) == False) {
         /* add element; the entry stays in the table as long as the process lives */
         const char *gettext_return_string = sge_gettext__((char *) msg_str);

         auto *new_mp = ocs::uti::create<sge_error_message_t>();
         DPRINTF_("add new hash table entry for message id: %d\n", msg_id);
         new_mp->id = msg_id;
         new_mp->category = 0;
         new_mp->counter = 1;
         new_mp->message = msg_str;
         if (msg_id <= 99999) {
            new_mp->local_message = "[" + std::to_string(msg_id) + "] " + gettext_return_string;
         } else {
            new_mp->local_message = gettext_return_string;
         }

         sge_htable_store(sge_message_hash_table, &key, new_mp);
         DRETURN_(new_mp->local_message.c_str());
      } else {
         /* check element */
         DPRINTF_("using old hash entry for message id: %d\n", msg_id);
         if (message_p.l->message != msg_str) {
            DPRINTF_("duplicate message id error: returning gettext() message");
            DPRINTF_("msg in : \"%s\"\n", msg_str);
            DPRINTF_("msg out: \"%s\"\n", message_p.l->message.c_str());
            DRETURN_(sge_gettext__((char *) msg_str));
         } else {
            message_p.l->counter = (message_p.l->counter) + 1;
            DPRINTF_("message count: " sge_u32 "\n", message_p.l->counter);
            DRETURN_(message_p.l->local_message.c_str());
         }
      }
   }
   DRETURN_(sge_gettext__((char *) msg_str));
#endif
}

/**
 * @brief Get translated message from message file
 *
 * makes a call to sge_language_functions.gettext_func(x) if
 * gettext_func is not nullptr, otherwise it returns the input
 * string.
 *
 * @param x pointer to message which should be internationalizied
 *
 * @return pointer internationalized message
 *
 * @note MT-NOTE: not guarded b/c sge_gettext__() is used only in infotext utility
 *
 * @see #sge_init_language, #sge_init_language_func, #sge_gettext, #sge_gettext_, #sge_gettext__, #sge_get_message_id_output, #sge_set_message_id_output
 */
const char *sge_gettext__(const char *x) {
   DENTER_(BASIS_LAYER);

   const char *z;
   if ((sge_language_functions.gettext_func != nullptr) &&
       (sge_are_language_functions_installed)) {
      z = sge_language_functions.gettext_func(x);
   } else {
      z = x;
      // do not use DPRINTF here - it will call this function recursively
      //DPRINTF_("sge_gettext() called without valid gettext function pointer!\n");
   }

   /*z = gettext(x);*/

   /*DPRINTF_("gettext: '%s' -> '%s'\n", x ? x : "", z ? z : "");*/

   DRETURN_(z);
}

/**
 * @brief Release the message id flag of an exiting thread
 *
 * Registered with `pthread_key_create()`; the flag was made with ocs::uti::create().
 *
 * @param theState the thread's int flag
 */
static void message_id_destroy(void *theState) {
   auto *flag = static_cast<int *>(theState);
   ocs::uti::destroy(flag);
}

/**
 * @brief Create the thread specific key of the message id flag and the flag of the calling thread
 *
 * Runs once per process through `pthread_once()`. Aborts if the flag cannot be stored.
 */
static void message_id_once_init() {
   pthread_key_create(&message_id_key, &message_id_destroy);

   int *buf = ocs::uti::create<int>();
   int res = pthread_setspecific(message_id_key, (const void *) buf);

   if (0 != res) {
      fprintf(stderr, "pthread_set_specific(%s) failed: %s\n", "log_buffer_getspecific", strerror(res));
      ocs::TerminationManager::trigger_abort();
   }
}

#endif
