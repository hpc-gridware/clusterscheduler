/*___INFO__MARK_BEGIN__*/
/*************************************************************************
 * 
 *  The Contents of thiz file are made available subject to the terms of
 *  the Sun Industry Standards Source License Version 1.2
 * 
 *  Sun Microsystems Inc., March, 2001
 * 
 * 
 *  Sun Industry Standards Source License Version 1.2
 *  =================================================
 *  The contents of thiz file are subject to the Sun Industry Standards
 *  Source License Version 1.2 (the "License"); You may not use thiz file
 *  except in compliance with the License. You may obtain a copy of the
 *  License at http://gridengine.sunsource.net/Gridengine_SISSL_license.html
 * 
 *  Software provided under thiz License is provided on an "AS IS" basis,
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
 *  Portions of this software are Copyright (c) 2023-2024 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <cstring>

#include "uti/sge_error_class.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include <sge_log.h>

typedef struct sge_error_message_str sge_error_message_t;

struct sge_error_message_str {
   int error_quality;
   int error_type;
   char *message;
   sge_error_message_t *next;
};

typedef struct sge_error_iterator_str sge_error_iterator_t;

struct sge_error_iterator_str {
   bool is_first_flag;
   sge_error_message_t *current;
};

typedef struct {
   sge_error_message_t *first;
   sge_error_message_t *last;
} sge_error_t;

static sge_error_iterator_class_t *sge_error_class_iterator(sge_error_class_t *thiz);

static bool sge_error_has_error(sge_error_class_t *eh);

static bool sge_error_has_quality(sge_error_class_t *thiz, int error_quality);

static bool sge_error_has_type(sge_error_class_t *thiz, int error_type);

static void sge_error_verror(sge_error_class_t *thiz, int error_type, int error_quality,
                             const char *format, va_list ap);

static void sge_error_error(sge_error_class_t *thiz, int error_type, int error_quality, const char *format, ...);

static void sge_error_clear(sge_error_t *et);

void sge_error_destroy(sge_error_t **t);

void sge_error_message_destroy(sge_error_message_t **elem);

static sge_error_iterator_class_t *sge_error_iterator_class_create(sge_error_class_t *ec);

static const char *sge_error_iterator_get_message(sge_error_iterator_class_t *thiz);

static u_long32 sge_error_iterator_get_quality(sge_error_iterator_class_t *thiz);

static u_long32 sge_error_iterator_get_type(sge_error_iterator_class_t *thiz);

static bool sge_error_iterator_next(sge_error_iterator_class_t *thiz);


sge_error_class_t *sge_error_class_create() {
   auto *ret = (sge_error_class_t *) sge_malloc(sizeof(sge_error_class_t));
   if (ret == nullptr) {
      return nullptr;
   }
   memset(ret, 0, sizeof(sge_error_class_t));

   ret->sge_error_handle = sge_malloc(sizeof(sge_error_t));
   SGE_ASSERT(ret->sge_error_handle != nullptr);
   memset(ret->sge_error_handle, 0, sizeof(sge_error_t));

   ret->has_error = sge_error_has_error;
   ret->has_quality = sge_error_has_quality;
   ret->has_type = sge_error_has_type;
   ret->error = sge_error_error;
   ret->verror = sge_error_verror;
   ret->clear = sge_error_class_clear;
   ret->iterator = sge_error_class_iterator;
   return ret;
}

static sge_error_iterator_class_t *sge_error_class_iterator(sge_error_class_t *thiz) {
   return sge_error_iterator_class_create(thiz);
}

void sge_error_class_destroy(sge_error_class_t **ec) {
   sge_error_t *et = nullptr;

   if (ec == nullptr || *ec == nullptr)
      return;

   et = (sge_error_t *) (*ec)->sge_error_handle;

   sge_error_destroy(&et);
   sge_free(ec);
}

void sge_error_class_clear(sge_error_class_t *thiz) {
   if (thiz != nullptr) {
      auto *et = (sge_error_t *) thiz->sge_error_handle;
      sge_error_clear(et);
   }
}

static void sge_error_clear(sge_error_t *et) {
   DENTER(TOP_LAYER);

   if (et != nullptr) {
      sge_error_message_t *elem = et->first;
      sge_error_message_t *next;
      while (elem != nullptr) {
         next = elem->next;
         sge_error_message_destroy(&elem);
         elem = next;
      }
      et->first = nullptr;
      et->last = nullptr;
   }

   DRETURN_VOID;
}


void sge_error_destroy(sge_error_t **t) {
   if (t == nullptr || *t == nullptr) {
      return;
   }

   sge_error_clear(*t);
   sge_free(t);
}


void sge_error_message_destroy(sge_error_message_t **elem) {
   if (elem == nullptr || *elem == nullptr) {
      return;
   }
   sge_free(&((*elem)->message));
   sge_free(elem);
}

static bool sge_error_has_error(sge_error_class_t *eh) {
   auto *et = (sge_error_t *) eh->sge_error_handle;
   return (et->first != nullptr) ? true : false;
}

static bool sge_error_has_quality(sge_error_class_t *thiz, int error_quality) {
   bool ret = false;

   DENTER(TOP_LAYER);
   if (thiz) {
      auto *et = (sge_error_t *) thiz->sge_error_handle;
      sge_error_message_t *elem = et->first;
      while (elem) {
         if (elem->error_quality == error_quality) {
            ret = true;
            break;
         }
      }
   }
   DRETURN(ret);
}

static bool sge_error_has_type(sge_error_class_t *thiz, int error_type) {
   bool ret = false;

   DENTER(TOP_LAYER);
   if (thiz) {
      auto *et = (sge_error_t *) thiz->sge_error_handle;
      sge_error_message_t *elem = et->first;
      while (elem) {
         if (elem->error_type == error_type) {
            ret = true;
            break;
         }
      }
   }
   DRETURN(ret);
}

static sge_error_iterator_class_t *sge_error_iterator_class_create(sge_error_class_t *ec) {
   DENTER(TOP_LAYER);

   auto *et = (sge_error_t *) ec->sge_error_handle;
   auto *elem = (sge_error_iterator_t *) sge_malloc(sizeof(sge_error_iterator_t));
   SGE_ASSERT(elem != nullptr);
   elem->current = et->first;
   elem->is_first_flag = true;

   auto ret = (sge_error_iterator_class_t *) sge_malloc(sizeof(sge_error_iterator_class_t));
   SGE_ASSERT(ret != nullptr);
   ret->sge_error_iterator_handle = elem;
   ret->get_message = sge_error_iterator_get_message;
   ret->get_quality = sge_error_iterator_get_quality;
   ret->get_type = sge_error_iterator_get_type;
   ret->next = sge_error_iterator_next;
   DRETURN(ret);
}

static void sge_error_verror(sge_error_class_t *thiz, int error_type, int error_quality,
                             const char *format, va_list ap) {

   auto *et = (sge_error_t *) thiz->sge_error_handle;
   dstring ds = DSTRING_INIT;

   DENTER(TOP_LAYER);

   auto *error = (sge_error_message_t *) sge_malloc(sizeof(sge_error_message_t));
   SGE_ASSERT(error != nullptr);
   error->error_quality = error_quality;
   error->error_type = error_type;

   sge_dstring_vsprintf(&ds, format, ap);

   error->message = strdup(sge_dstring_get_string(&ds));
   error->next = nullptr;
   sge_dstring_free(&ds);

   DPRINTF("error: %s\n", error->message ? error->message : "");

   if (et->first == nullptr) {
      et->first = error;
      et->last = error;
   } else {
      et->last->next = error;
      et->last = error;
   }

   DRETURN_VOID;
}

static void
sge_error_error(sge_error_class_t *thiz, int error_type, int error_quality, const char *format, ...) {
   DENTER(TOP_LAYER);
   if (format != nullptr) {
      va_list ap;

      va_start(ap, format);
      sge_error_verror(thiz, error_type, error_quality, format, ap);
      va_end(ap);
   }
   DRETURN_VOID;
}


void sge_error_iterator_class_destroy(sge_error_iterator_class_t **thiz) {
   sge_error_iterator_t *elem = nullptr;

   if (!thiz) {
      return;
   }

   elem = (sge_error_iterator_t *) (*thiz)->sge_error_iterator_handle;

   sge_free(&elem);
   sge_free(thiz);
}

static const char *sge_error_iterator_get_message(sge_error_iterator_class_t *thiz) {
   auto *elem = (sge_error_iterator_t *) thiz->sge_error_iterator_handle;

   if (elem && elem->current) {
      return elem->current->message;
   }
   return nullptr;
}

static u_long32 sge_error_iterator_get_quality(sge_error_iterator_class_t *thiz) {
   auto *elem = (sge_error_iterator_t *) thiz->sge_error_iterator_handle;

   if (elem && elem->current) {
      return elem->current->error_quality;
   }
   return -1;
}

static u_long32 sge_error_iterator_get_type(sge_error_iterator_class_t *thiz) {
   auto *elem = (sge_error_iterator_t *) thiz->sge_error_iterator_handle;

   if (elem && elem->current) {
      return elem->current->error_type;
   }
   return -1;
}

static bool sge_error_iterator_next(sge_error_iterator_class_t *thiz) {
   auto *elem = (sge_error_iterator_t *) thiz->sge_error_iterator_handle;

   if (!elem) {
      return false;
   }
   if (elem->is_first_flag) {
      elem->is_first_flag = false;
      return (elem->current != nullptr) ? true : false;
   }
   if (elem->current) {
      elem->current = elem->current->next;

      return (elem->current != nullptr) ? true : false;
   }
   return false;
}
