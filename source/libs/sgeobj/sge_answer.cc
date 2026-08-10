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
 * @brief The answer list: how errors, warnings and infos travel back to the caller
 *
 * An answer element (`AN_Type`) pairs a message with a status and a quality.
 * Callees append to an answer list instead of returning an error code, so a
 * single call can report several problems and GDI can carry them from the
 * server to the client unchanged.
 *
 * @code
 * void caller() {
 *    lList *answer_list = nullptr;
 *
 *    callee(&answer_list);
 *    if (answer_list_has_error(&answer_list)) {
 *       try_to_handle_error();
 *       lFreeList(&answer_list);
 *    }
 * }
 *
 * void callee(lList **answer_list) {
 *    char *s = sge_malloc(256);
 *
 *    if (s == nullptr) {
 *       answer_list_add(answer_list, "no memory",
 *                       STATUS_ERROR, ANSWER_QUALITY_ERROR);
 *       return;
 *    }
 * }
 * @endcode
 *
 * @note MT-NOTE: the answer list module is MT safe
 */

#include <cstdarg>
#include <cstring>

#include "cull/cull.h"

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_unistd.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/msg_sgeobjlib.h"

/// Debug layer the answer list traces are written to
#define ANSWER_LAYER CULL_LAYER

static bool answer_is_recoverable(const lListElem *answer);
static bool answer_log(const lListElem *answer, bool show_info);

/**
 * @brief Check for certain answer quality
 *
 * Return true (1) if "answer" has the given "quality"
 *
 * @param answer AN_Type element
 * @param quality Quality id
 *
 * @return true or false
 *
 * @note MT-NOTE: answer_has_quality() is MT safe
 */
bool answer_has_quality(const lListElem *answer, answer_quality_t quality) {
   DENTER(ANSWER_LAYER);

   bool ret;

   ret = (lGetUlong(answer, AN_quality) ==  quality) ? true : false;
   DRETURN(ret);
}

/**
 * @brief Check for recoverable error
 *
 * This function return true (1) if "answer" is an error where the
 * calling function application may recover from.
 * Following error are handeled as nonrecoverable:
 *
 *    STATUS_NOQMASTER
 *    STATUS_NOCOMMD
 *    STATUS_ENOKEY
 *
 * @param answer AN_Type element
 *
 * @return true or false
 *
 * @note MT-NOTE: answer_is_recoverable() is MT safe
 */
static bool answer_is_recoverable(const lListElem *answer) {
   DENTER(ANSWER_LAYER);

   bool ret = true;

   if (answer != nullptr) {
      const int max_non_recoverable = 4;
      const uint32_t non_recoverable[] = {
         STATUS_NOQMASTER,
         STATUS_NOCOMMD,
         STATUS_ENOKEY,
         STATUS_NOCONFIG
      };
      uint32_t status = lGetUlong(answer, AN_status);
      int i;

      for (i = 0; i < max_non_recoverable; i++) {
         if (status == non_recoverable[i]) {
            ret = false;
            break;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Exit on certain errors
 *
 * This function checks if "answer" is an unrecoverable error.
 * (answer_is_recoverable() is used to check this.) The error
 * text will then be printed to stderr and the calling
 * process will be terminated with exit code 1.
 *
 * @param answer AN_Type element
 *
 * @note This function may never return.
 *
 *       MT-NOTE: answer_exit_if_not_recoverable() is MT safe
 */
void answer_exit_if_not_recoverable(const lListElem *answer) {
   DENTER(ANSWER_LAYER);
   if (!answer_is_recoverable(answer)) {
      fprintf(stderr, "%s: %s\n", answer_get_quality_text(answer),
              lGetString(answer, AN_text));
      sge_exit(1);
   }
   DRETURN_VOID;
}

/**
 * @brief Get quality text
 *
 * Returns a string representation for the quality of the "answer"
 *
 * @param answer AN_Type list
 *
 * @return String
 *
 * @note MT-NOTE: answer_get_quality_text() is MT safe
 */
const char *answer_get_quality_text(const lListElem *answer) {
   DENTER(ANSWER_LAYER);

   const char *quality_text[] = {
      "CRITICAL",
      "ERROR",
      "WARNING",
      "INFO"
   };
   uint32_t quality;

   quality = lGetUlong(answer, AN_quality);
   if (quality >= ANSWER_QUALITY_END) {
      quality = ANSWER_QUALITY_CRITICAL;
   }
   DRETURN(quality_text[quality]);
}

/**
 * @brief Return the error status
 *
 * Return the error status of "answer".
 *
 * @param answer AN_Type element
 *
 * @return error status
 *
 * @note MT-NOTE: answer_get_status() is MT safe
 */
uint32_t answer_get_status(const lListElem *answer) {
   DENTER(ANSWER_LAYER);

   uint32_t ret;

   ret = lGetUlong(answer, AN_status);
   DRETURN(ret);
}

/**
 * @brief Prints error text
 *
 * Prints "prefix", the error text of "answer" and "suffix"
 * to "stream".
 *
 * @param answer AN_Type element
 * @param stream Output stream
 * @param prefix text printed before the message
 * @param suffix text printed after the message
 *
 * @note MT-NOTE: answer_print_text() is MT safe
 */
void answer_print_text(const lListElem *answer,
                       FILE *stream,
                       const char *prefix,
                       const char *suffix) {
   DENTER(ANSWER_LAYER);

   const char *text = nullptr;

   text = lGetString(answer, AN_text);

   if (prefix != nullptr) {
      fprintf(stream, "%s", prefix);
   }
   if (text != nullptr) {
      fprintf(stream, "%s", text);
   }
   if (suffix != nullptr) {
      fprintf(stream, "%s", suffix);
   }
   fprintf(stream, "\n");
   DRETURN_VOID;
}

/**
 * @brief Copy answer to dstring without newline
 *
 * Copy answer text into dstring without newline character.
 *
 * @param answer AN_Type element
 * @param diag destination dstring
 *
 * @note MT-NOTE: answer_to_dstring() is MT safe
 */
void answer_to_dstring(const lListElem *answer, dstring *diag) {
   if (diag) {
      if (!answer) {
         sge_dstring_copy_string(diag, MSG_ANSWERWITHOUTDIAG);
      } else {
         const char *s, *t;
         s = lGetString(answer, AN_text);
         if ((t=strchr(s, '\n'))) {
            sge_dstring_sprintf_append(diag, "%.*s", t-s, s); 
         }
         else {
            sge_dstring_append(diag, s);
         }   
      }
   }
}

/**
 * @brief Copy answer to dstring without newline
 *
 * Copy answer list text into dstring with each element separated by a
 * newline character.
 *
 * @param alp AN_Type list
 * @param diag destination dstring
 *
 * @note MT-NOTE: answer_list_to_dstring() is MT safe
 */
void answer_list_to_dstring(const lList *alp, dstring *diag) {
   if (diag) {
      if (!alp || (lGetNumberOfElem (alp) == 0)) {
         sge_dstring_copy_string(diag, MSG_ANSWERWITHOUTDIAG);
      } else {
         sge_dstring_clear(diag);
         
         for_each_ep_lv(aep, alp) {
            const char *s = lGetString(aep, AN_text);
            sge_dstring_append(diag, s);

            if (strchr(s, '\n') == nullptr) {
               sge_dstring_append_char(diag, '\n');
            }
         }
      }
   }
}

/**
 * @brief Format add an answer to an answer list
 *
 * This function creates a new answer element having the properties
 * "status", "quality", and a message text created from fmt and the
 * following variable argument list.
 *
 * The new element will be appended to "answer_list".
 *
 * If "answer_list" is nullptr, no action is performed.
 *
 * If the list pointed to by "answer_list" is nullptr, a new list will be
 * created.
 *
 * @param answer_list AN_Type list
 * @param status answer status
 * @param quality answer quality
 * @param fmt format string to create message (printf) ...                      - arguments used for formatting message
 *
 * @return true on success, else false
 *
 * @note MT-NOTE: answer_list_add_sprintf() is MT safe
 *
 * @see #answer_list_add
 */
bool answer_list_add_sprintf(lList **answer_list, uint32_t status,
                             answer_quality_t quality, const char *fmt, ...) {
   DENTER(ANSWER_LAYER);

   bool ret = false;

   if (answer_list != nullptr) {
      dstring buffer = DSTRING_INIT;
      const char *message;
      va_list ap;

      va_start(ap, fmt);
      message = sge_dstring_vsprintf(&buffer, fmt, ap);
      va_end(ap);

      if (message != nullptr) {
         ret = answer_list_add(answer_list, message, status, quality);
      }

      sge_dstring_free(&buffer);
   }

   DRETURN(ret);
}

/**
 * @brief Contains list
 *
 * The function returns true (1) if the "answer_list" contains
 * at least one answer element with the given "quality".
 *
 * @param answer_list AN_Type list
 * @param quality quality value
 *
 * @return true or false
 *
 * @note MT-NOTE: answer_list_has_quality() is MT safe
 */
bool answer_list_has_quality(lList **answer_list, answer_quality_t quality) {
   DENTER(ANSWER_LAYER);

   bool ret = false;

   if (answer_list != nullptr) {
      for_each_ep_lv(answer, *answer_list) {
         if (answer_has_quality(answer, quality)) {
            ret = true;
            break;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Remove elements from list
 *
 * The function removes all answer list elements with the given quality from
 * the list.
 *
 * @param answer_list AN_Type list
 * @param quality quality value
 *
 * @note MT-NOTE: answer_list_remove_quality() is MT safe
 */
void answer_list_remove_quality(lList *answer_list, answer_quality_t quality) {
   DENTER(ANSWER_LAYER);

   lListElem *aep, *nxt = lFirstRW(answer_list);

   while ((aep=nxt)) {
      nxt=lNextRW(aep);
      if (lGetUlong(aep, AN_quality) == quality) {
         lRemoveElem(answer_list, &aep);
      }
   }

   DRETURN_VOID;
}


/**
 * @brief Status contains in list
 *
 * The function returns true if the "answer_list" contains at least
 * one answer element with the given status
 *
 * @param answer_list AN_Type list
 * @param status expected status
 *
 * @return true or false
 *
 * @note MT-NOTE: answer_list_has_status() is MT safe
 */
bool answer_list_has_status(lList **answer_list, uint32_t status) {
   DENTER(ANSWER_LAYER);

   bool ret = false;

   if (answer_list != nullptr) {
      for_each_ep_lv(answer, *answer_list) {
         if (answer_get_status(answer) == status) {
            ret = true;
            break;
         }
      }
   }
   DRETURN(ret);
}

/**
 * @brief Is an "error " in the list
 *
 * The function returns true (1) if the "answer_list" containes
 * at least one error answer element
 *
 * @param answer_list AN_Type list
 *
 * @return true or false
 *
 * @note MT-NOTE: answer_list_has_error() is MT safe
 */
bool answer_list_has_error(lList **answer_list) {
   DENTER(ANSWER_LAYER);

   bool ret = false;

   if ((answer_list_has_quality(answer_list, ANSWER_QUALITY_ERROR)) ||
       (answer_list_has_quality(answer_list, ANSWER_QUALITY_CRITICAL))) {
       ret = true;
   }
   DRETURN(ret);
}

/**
 * @brief Print and/or exit
 *
 * The error texts of all answer elements within "answer_list" are
 * printed to "stream". If an unrecoverable error is detected
 * the calling process will be terminated.
 *
 * @param answer_list AN_Type list
 * @param stream output stream
 *
 * @note MT-NOTE: answer_list_on_error_print_or_exit() is MT safe
 */
void answer_list_on_error_print_or_exit(lList **answer_list, FILE *stream) {
   DENTER(ANSWER_LAYER);
   for_each_ep_lv(answer, *answer_list) {
      answer_exit_if_not_recoverable(answer);
      answer_print_text(answer, stream, nullptr, nullptr);
   }
   DRETURN_VOID;
}

/**
 * @brief Print and/or exit
 *
 * Prints all messages contained in "answer_list". All error
 * messages will be printed to stderr with an an initial "err_prefix".
 * All warning and info messages will be printed to stdout with
 * the prefix "warn_prefix".
 *
 * If the "answer_list" contains at least one error then this
 * function will return with a positive return value. This value
 * is the errror status of the first error message.
 *
 * If there is no error contained in 'answer_list' than this function
 * will return with a value of 0.
 *
 * "*answer_list" will be freed.
 *
 * @param answer_list AN_Type list
 * @param critical_prefix prefix for critical messages, e.g. "qsub: "
 * @param err_prefix e.g. "qsub: "
 * @param warn_prefix e.g. MSG_WARNING
 * @return the status of the first error message, or 0 when there was none
 *
 * @note MT-NOTE: answer_list_print_err_warn() is MT safe
 */
int answer_list_print_err_warn(lList **answer_list,
                               const char *critical_prefix,
                               const char *err_prefix,
                               const char *warn_prefix) {
   DENTER(ANSWER_LAYER);

   int do_exit = 0;
   uint32_t status = 0;

   for_each_ep_lv(answer, *answer_list) {
      if (answer_has_quality(answer, ANSWER_QUALITY_CRITICAL)) {
         answer_print_text(answer, stderr, critical_prefix, nullptr);
         if (do_exit == 0) {
            status = answer_get_status(answer);
            do_exit = 1;
         }
      } else if (answer_has_quality(answer, ANSWER_QUALITY_ERROR)) {
         answer_print_text(answer, stderr, err_prefix, nullptr);
         if (do_exit == 0) {
            status = answer_get_status(answer);
            do_exit = 1;
         }
      } else if (answer_has_quality (answer, ANSWER_QUALITY_WARNING)) {
         answer_print_text(answer, stdout, warn_prefix, nullptr);
      }
      else {
         answer_print_text(answer, stdout, nullptr, nullptr);
      }
   }
   lFreeList(answer_list);
   DRETURN((int)status);
}

/**
 * @brief Handle res. of request
 *
 * Processes the answer list that results from a gdi request
 * (sge_gdi or ocs::gdi::Client::sge_gdi_multi).
 * Outputs and errors and warnings and returns the first error
 * or warning status code.
 * The answer list is freed.
 *
 * @param answer_list answer list to process
 * @param stream output stream
 *
 * @return first error or warning status code or STATUS_OK
 *
 * @note MT-NOTE: answer_list_handle_request_answer_list() is MT safe
 */
int answer_list_handle_request_answer_list(lList **answer_list, FILE *stream) {
   DENTER(ANSWER_LAYER);

   int ret = STATUS_OK;

   if(answer_list != nullptr && *answer_list != nullptr) {
      const lListElem *answer;

      for_each_ep(answer, *answer_list) {
         if(answer_has_quality(answer, ANSWER_QUALITY_CRITICAL) ||
            answer_has_quality(answer, ANSWER_QUALITY_ERROR) ||
            answer_has_quality(answer, ANSWER_QUALITY_WARNING)) {
            answer_print_text(answer, stream, nullptr, nullptr);
            if(ret == STATUS_OK) {
               ret = lGetUlong(answer, AN_status);
            }
         }
      }
      lFreeList(answer_list);
   } else {
      fprintf(stream, "%s\n", MSG_ANSWER_NOANSWERLIST);
      return STATUS_EUNKNOWN;
   }
   DRETURN(ret);
}

/**
 * @brief Add an answer to an answer list
 *
 * This function creates a new answer element (using "quality",
 * "status" and "text"). The new element will be appended to
 * "answer_list"
 *
 * If "answer_list" is nullptr, no action is performed.
 *
 * If the list pointed to by "answer_list" is nullptr, a new list will be
 * created.
 *
 * @param answer_list AN_Type list
 * @param text answer text
 * @param status answer status
 * @param quality answer quality
 *
 * @return error state true  - OK false - error occurred
 *
 * @note MT-NOTE: answer_list_add() is MT safe
 *
 * @see #answer_list_add_sprintf
 */
bool answer_list_add(lList **answer_list, const char *text,
                     uint32_t status, answer_quality_t quality) {
   DENTER(ANSWER_LAYER);

   bool ret = false;

   if (answer_list != nullptr) {
      lListElem *answer = lCreateElem(AN_Type);

      if (answer != nullptr) {
         lSetString(answer, AN_text, text);
         lSetUlong(answer, AN_status, status);
         lSetUlong(answer, AN_quality, quality);

         if (*answer_list == nullptr) {
            *answer_list = lCreateList("", AN_Type);
         }

         if (*answer_list != nullptr) {
            lAppendElem(*answer_list, answer);
            ret = true;
         }
      }

      if (!ret) {
         lFreeElem(&answer);
      }
   }
   DRETURN(ret);
}

/**
 * @brief Append an existing answer element to an answer list
 *
 * The list is created when it does not exist yet.
 *
 * @param[in,out] answer_list the list to append to
 * @param answer the element to append; ownership passes to the list
 * @return true when the element was appended
 *
 * @note MT-NOTE: answer_list_add_elem() is MT safe
 */
bool answer_list_add_elem(lList **answer_list, lListElem *answer) {
   DENTER(ANSWER_LAYER);

   bool ret = false;

   if (answer_list != nullptr) {
      if (*answer_list == nullptr) {
         *answer_list = lCreateList("", AN_Type);
      }
      if (*answer_list != nullptr) {
         lAppendElem(*answer_list, answer);
         ret = true;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Replace an answer list
 *
 * free *answer_list and replace it by *new_list.
 *
 * @param answer_list AN_Type
 * @param new_list AN_Type
 *
 * @note MT-NOTE: answer_list_replace() is MT safe
 */
void answer_list_replace(lList **answer_list, lList **new_list) {
   DENTER(ANSWER_LAYER);
   if (answer_list != nullptr) {
      lFreeList(answer_list);

      if (new_list != nullptr) {
         *answer_list = *new_list; 
         *new_list = nullptr;
      } else {
         *answer_list = nullptr;
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Append two lists
 *
 * Append "new_list" after "answer_list". *new_list will be nullptr afterwards
 *
 * @param answer_list AN_Type list
 * @param new_list AN_Type list
 */
void answer_list_append_list(lList **answer_list, lList **new_list) {
   DENTER(ANSWER_LAYER);
   if (answer_list != nullptr && new_list != nullptr) {
      if (*answer_list == nullptr && *new_list != nullptr) {
         *answer_list = lCreateList("", AN_Type);
      }
      if (*new_list != nullptr) {
         lAddList(*answer_list, new_list);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief Output and free answer_list
 *
 * Prints all messages contained in "answer_list".
 * The ERROR, WARNING and INFO macros will be used for output.
 *
 * If the "answer_list" contains at least one error then this
 * function will return true.
 *
 * If there is no error contained in 'answer_list' then this function
 * will return with a value of false.
 *
 * "*answer_list" will only be freed and set to nullptr, if is_free_list is
 * true.
 *
 * @param answer_list AN_Type list
 * @param is_free_list if true, frees the answer list
 * @param show_info log also info messages
 * @return true when the list contained at least one error
 *
 * @note MT-NOTE: answer_list_print_err_warn() is MT safe
 */
bool answer_list_log(lList **answer_list, bool is_free_list, bool show_info) {
   DENTER(ANSWER_LAYER);

   bool ret = false;
   const lListElem *answer;   /* AN_Type */

   if (answer_list != nullptr && *answer_list != nullptr) {
      for_each_ep(answer, *answer_list) {
         ret = answer_log(answer, show_info);
      }
      if (is_free_list) {
         lFreeList(answer_list);
      }
   }

   DRETURN(ret);
}

/**
 * @brief Output answer
 *
 * Prints the message contained in "answer".
 * The CRITICAL, ERROR, WARNING and INFO macros will be used for output.
 *
 * @param answer AN_Type element
 *
 * @return true if answer is an error, false otherwise and if answer == nullptr
 *
 * @note MT-NOTE: answer_log() is MT safe
 */
static bool answer_log(const lListElem *answer, bool show_info) {
   DENTER(ANSWER_LAYER);

   bool ret = false;

   if (!answer) {
      DRETURN(ret);
   }

   switch (lGetUlong(answer, AN_quality)) {
      case ANSWER_QUALITY_CRITICAL:
         CRITICAL(SFNMAX, lGetString(answer, AN_text));
         ret = true;
         break;
      case ANSWER_QUALITY_ERROR:
         ERROR(SFNMAX, lGetString(answer, AN_text));
         ret = true;
         break;
      case ANSWER_QUALITY_WARNING:
         WARNING(SFNMAX, lGetString(answer, AN_text));
         break;
      case ANSWER_QUALITY_INFO:
         if (show_info) {
            INFO(SFNMAX, lGetString(answer, AN_text));
         }
         break;
      default:
         break;
   }

   DRETURN(ret);
}

/**
 * @brief Output and free answer_list
 *
 * Prints all messages contained in "answer_list".
 * The ERROR, WARNING and INFO macros will be used for output.
 *
 * If the "answer_list" contains at least one error then this
 * function will return true.
 *
 * If there is no error contained in 'answer_list' then this function
 * will return with a value of false.
 *
 * "*answer_list" will be freed and set to nullptr.
 *
 * @param answer_list AN_Type list
 * @return true when the list contained at least one error
 *
 * @note MT-NOTE: answer_list_output() is MT safe
 */
bool answer_list_output(lList **answer_list) {
   return answer_list_log(answer_list, true, true);
}


/**
 * @brief Print the last message of an answer list to stderr
 *
 * Exits first if any element reports an unrecoverable condition.
 *
 * @param alp AN_Type list
 * @return 1 when any element carried a status other than `STATUS_OK`, else 0
 */
int show_answer(lList *alp) {
   DENTER(TOP_LAYER);

   const lListElem *aep = nullptr;
   int ret = 0;
   
   if (alp != nullptr) {
    
      for_each_ep(aep,alp) {
         answer_exit_if_not_recoverable(aep);
         if (lGetUlong(aep, AN_status) != STATUS_OK) {
            ret = 1;
         }
      }
      aep = lLast(alp);
      if (lGetUlong(aep, AN_quality) != ANSWER_QUALITY_END) {
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      }
   }
   
   DRETURN(ret);
}

/**
 * @brief Print every message of an answer list to stderr
 *
 * Exits first if any element reports an unrecoverable condition.
 *
 * @param alp AN_Type list
 * @return 1 when any element carried a status other than `STATUS_OK`, else 0
 */
int show_answer_list(lList *alp) {
   DENTER(TOP_LAYER);

   const lListElem *aep = nullptr;
   int ret = 0;
   
   if (alp != nullptr) {
      for_each_ep(aep,alp) {
         if (lGetUlong(aep, AN_quality) == ANSWER_QUALITY_END) {
            continue;
         }

         answer_exit_if_not_recoverable(aep);
         if (lGetUlong (aep, AN_status) != STATUS_OK) {
            ret = 1;
         }
         fprintf(stderr, "%s\n", lGetString(aep, AN_text));
      }
   }
   
   DRETURN(ret);
}

/**
 * @brief Turn the errors collected in an error handler into answer elements
 *
 * @param eh the error handler to read; nullptr is ignored
 * @param[out] alpp receives one element per error; nullptr is ignored
 * @param clear_errors true to drop the errors from `eh` afterwards
 */
void answer_list_from_sge_error(sge_error_class_t *eh, lList **alpp, bool clear_errors) {
   sge_error_iterator_class_t *iter = nullptr;
   
   if (eh == nullptr || alpp == nullptr) {
      return;
   }
   iter = eh->iterator(eh);
   while (iter && iter->next(iter)) {
      answer_list_add(alpp, iter->get_message(iter), iter->get_type(iter), (answer_quality_t)iter->get_quality(iter));
   }
   if (clear_errors) {
      sge_error_class_clear(eh);
   }

   sge_error_iterator_class_destroy(&iter);
}
