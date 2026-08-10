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
 * @brief Implementation of the spooling directory layout handling
 */

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <cerrno>
#include <fcntl.h>

#include "uti/msg_utilib.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_spool.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "sge.h"

/// array tasks spooled into one directory before a new one is started
#define MAX_JA_TASKS_PER_DIR  (4096l)
/// array tasks stored in one spool file
#define MAX_JA_TASKS_PER_FILE (1l)

static int silent_flag = 0;

static washing_machine_t wtype;

static const char *spoolmsg_message[] = {
        "",
        "DO NOT MODIFY THIS FILE MANUALLY!",
        "",
        nullptr
};

static void get_spool_dir_range(uint32_t ja_task_id, uint32_t *start,
                                uint32_t *end) {
   uint32_t row = (ja_task_id - 1) / sge_get_ja_tasks_per_directory();

   *start = row * sge_get_ja_tasks_per_directory() + 1;
   *end = (row + 1) * sge_get_ja_tasks_per_directory();
}

static void get_spool_dir_parts(uint32_t job_id, char *first, size_t first_size,
                                char *second, size_t second_size,
                                char *third, size_t third_size) {
   snprintf(third, first_size, "%04d", (int) (job_id % 10000l));
   job_id /= 10000l;
   snprintf(second, second_size, "%04d", (int) (job_id % 10000l));
   job_id /= 10000l;
   snprintf(first, third_size, "%02d", (int) (job_id % 10000l));
}

/**
 * @brief Configured number of tasks per dir
 *
 * Returns the configured number of tasks per directory
 *
 * @return the number
 *
 * @note MT-NOTE: sge_get_ja_tasks_per_directory() is not MT safe
 */
uint32_t sge_get_ja_tasks_per_directory() {
   static uint32_t tasks_per_directory = 0;

   if (!tasks_per_directory) {
      char *env_string;

      env_string = getenv("SGE_MAX_TASKS_PER_DIRECTORY");
      if (env_string) {
         tasks_per_directory = (uint32_t) strtol(env_string, nullptr, 10);
      }
   }
   if (!tasks_per_directory) {
      tasks_per_directory = MAX_JA_TASKS_PER_DIR;
   }
   return tasks_per_directory;
}

/**
 * @brief Configured number of tasks per file
 *
 * Returns the configured number of tasks per file
 *
 * @return the number
 *
 * @note MT-NOTE: sge_get_ja_tasks_per_file() is not MT safe
 */
uint32_t sge_get_ja_tasks_per_file() {
   static uint32_t tasks_per_file = 0;

   if (!tasks_per_file) {
      char *env_string;

      env_string = getenv("SGE_MAX_TASKS_PER_FILE");
      if (env_string) {
         tasks_per_file = (uint32_t) strtol(env_string, nullptr, 10);
      }
   }
   if (!tasks_per_file) {
      tasks_per_file = MAX_JA_TASKS_PER_FILE;
   }
   return tasks_per_file;
}

/**
 * @brief Return SGE/EE specific file/pathname
 *
 * ???
 *
 * @param buffer buffer receiving the path
 * @param buffer_size size of @p buffer in bytes
 * @param id which file or directory to build, see #sge_file_path_id_t
 * @param format_flags how much of the path to return, see #sge_file_path_format_t
 * @param spool_flags context the name is needed in, see #sge_spool_flags_t
 * @param ulong_val1 1st ulong
 * @param ulong_val2 2nd ulong
 * @param string_val1 1st string
 *
 * @return equivalent with 'buffer'
 *
 * @note MT-NOTE: sge_get_file_path() is not MT safe due to get_spool_dir_range()
 */
char *sge_get_file_path(char *buffer, size_t buffer_size, sge_file_path_id_t id,
                        sge_file_path_format_t format_flags,
                        sge_spool_flags_t spool_flags,
                        uint32_t ulong_val1, uint32_t ulong_val2,
                        const char *string_val1) {
   int first_part = format_flags & FORMAT_FIRST_PART;
   int second_part = format_flags & FORMAT_SECOND_PART;
   int third_part = format_flags & FORMAT_THIRD_PART;
   int insert_dot = format_flags & FORMAT_DOT_FILENAME;
   int in_execd = spool_flags & SPOOL_WITHIN_EXECD;
   const char *spool_dir = JOB_DIR;

   if (id == JOBS_SPOOL_DIR) {
      snprintf(buffer, buffer_size, SFN, spool_dir);
   } else if (id == JOB_SPOOL_DIR || id == JOB_SPOOL_FILE ||
              id == TASKS_SPOOL_DIR || id == TASK_SPOOL_DIR_AS_FILE ||
              id == TASK_SPOOL_DIR || id == JOB_SPOOL_DIR_AS_FILE ||
              id == TASK_SPOOL_FILE || id == PE_TASK_SPOOL_FILE) {
      dstring dstr_job_dir = DSTRING_INIT;
      const char *job_dir;
      const char *file_prefix = "";
      char id_range[SGE_PATH_MAX] = "";
      char job_dir_first[SGE_PATH_MAX] = "";
      char job_dir_second[SGE_PATH_MAX] = "";
      char job_dir_third[SGE_PATH_MAX] = "";

      get_spool_dir_parts(ulong_val1, job_dir_first, SGE_PATH_MAX, job_dir_second, SGE_PATH_MAX,
                          job_dir_third, SGE_PATH_MAX);

      if (first_part) {
         // @todo: can this be correct?
         job_dir = "";
      } else if (second_part) {
         job_dir = sge_dstring_sprintf(&dstr_job_dir, "%s", job_dir_first);
      } else if (third_part) {
         job_dir = sge_dstring_sprintf(&dstr_job_dir, "%s/%s", job_dir_first, job_dir_second);
      } else {
         if (id == JOB_SPOOL_DIR_AS_FILE && insert_dot) {
            if (in_execd) {
               job_dir = sge_dstring_sprintf(&dstr_job_dir, "%s/%s/.%s." sge_u32, job_dir_first,
                                             job_dir_second, job_dir_third, ulong_val2);
            } else {
               job_dir = sge_dstring_sprintf(&dstr_job_dir, "%s/%s/.%s", job_dir_first,
                                             job_dir_second, job_dir_third);
            }
         } else {
            if (in_execd) {
               job_dir = sge_dstring_sprintf(&dstr_job_dir, "%s/%s/%s." sge_u32, job_dir_first,
                                             job_dir_second, job_dir_third, ulong_val2);
            } else {
               job_dir = sge_dstring_sprintf(&dstr_job_dir, "%s/%s/%s", job_dir_first,
                                             job_dir_second, job_dir_third);
            }
         }
      }
      if (insert_dot && (id == JOB_SPOOL_FILE || id == TASK_SPOOL_DIR_AS_FILE ||
                         id == TASK_SPOOL_FILE || id == PE_TASK_SPOOL_FILE)) {
         file_prefix = ".";
      }
      if (id == TASKS_SPOOL_DIR || id == TASK_SPOOL_DIR_AS_FILE ||
          id == TASK_SPOOL_DIR || id == TASK_SPOOL_FILE ||
          id == PE_TASK_SPOOL_FILE) {
         uint32_t start, end;
         get_spool_dir_range(ulong_val2, &start, &end);
         snprintf(id_range, sizeof(id_range), sge_u32"-" sge_u32, start, end);
      }
      if (id == JOB_SPOOL_DIR || id == JOB_SPOOL_DIR_AS_FILE) {
         snprintf(buffer, buffer_size, "%s/%s", spool_dir, job_dir);
      } else if (id == JOB_SPOOL_FILE) {
         snprintf(buffer, buffer_size, "%s/%s/%s%s", spool_dir, job_dir, file_prefix, "common");
      } else if (id == TASKS_SPOOL_DIR) {
         snprintf(buffer, buffer_size, "%s/%s/%s", spool_dir, job_dir, id_range);
      } else if (id == TASK_SPOOL_DIR_AS_FILE || id == TASK_SPOOL_DIR) {
         snprintf(buffer, buffer_size, "%s/%s/%s/%s" sge_u32, spool_dir, job_dir, id_range, file_prefix, ulong_val2);
      } else if (id == TASK_SPOOL_FILE) {
         snprintf(buffer, buffer_size, "%s/%s/%s/" sge_u32 "/%s%s", spool_dir, job_dir, id_range, ulong_val2, file_prefix, "common");
      } else if (id == PE_TASK_SPOOL_FILE) {
         snprintf(buffer, buffer_size, "%s/%s/%s/" sge_u32 "/%s%s", spool_dir, job_dir, id_range, ulong_val2, file_prefix, string_val1);
      }
      sge_dstring_free(&dstr_job_dir);
   } else if (id == JOB_SCRIPT_DIR) {
      snprintf(buffer, buffer_size, "%s", EXEC_DIR);
   } else if (id == JOB_SCRIPT_FILE) {
      snprintf(buffer, buffer_size, "%s/" sge_u32, EXEC_DIR, ulong_val1);
   } else if (id == JOB_ACTIVE_DIR && in_execd) {
      snprintf(buffer, buffer_size, ACTIVE_DIR "/" sge_u32 "." sge_u32, ulong_val1, ulong_val2);
   } else {
      buffer[0] = '\0';
   }

   return buffer;
}

/**
 * @brief Add a comment in a file
 *
 * This function writes an additional comment into a file. First
 * character in a comment line is 'comment_char'.
 *
 * @param file file to write to
 *
 * @return -1 on error else 0
 *
 * @note MT-NOTE: sge_spoolmsg_write() is not MT safe due to FPRINTF() macro
 * @param comment_char character introducing the comment line
 * @param version version string written into the comment
*/
int sge_spoolmsg_write(FILE *file, char comment_char, const char *version) {
   int i;

   FPRINTF((file, "%c Version: %s\n", comment_char, version));
   i = 0;
   while (spoolmsg_message[i]) {
      FPRINTF((file, "%c %s\n", comment_char, spoolmsg_message[i]));
      i++;
   }

   return 0;
   FPRINTF_ERROR:
   return -1;
}

/** @brief Add a comment line to a spool file, C++ stream variant
 *
 * @param stream stream to write to
 * @param comment_char character introducing the comment line
 * @param version version string written into the comment
 * @return 0 on success, -1 on write error
 */
int sge_spoolmsg_write(std::ofstream &stream, char comment_char, const char *version) {
   stream << comment_char << " Version: " << version << '\n';
   for (int i = 0; spoolmsg_message[i] != nullptr; ++i) {
      stream << comment_char << spoolmsg_message[i] << '\n';
   }

   return stream.fail() ? -1 : 0;
}

/** @brief Append a spool file comment line to a dstring
 *
 * @param ds the dstring to append to
 * @param comment_char character introducing the comment line
 * @param version version string written into the comment
 */
void sge_spoolmsg_append(dstring *ds, char comment_char, const char *version) {
   int i = 0;

   sge_dstring_sprintf_append(ds, "%c Version: %s\n", comment_char, version);
   while (spoolmsg_message[i]) {
      sge_dstring_sprintf_append(ds, "%c %s\n", comment_char, spoolmsg_message[i]);
      i++;
   }
}

/**
 * @brief Read pid from file
 *
 * Read pid from file 'fname'. The pidfile may be terminated with
 * a '\n'. Empty lines at the beginning of the file are ignored.
 * Whitespaces at the beginning of the line are ignored.
 * Any characters or lines after a valid pid are ignored.
 *
 * @param fname filename
 *
 * @return process id
 *
 * @note MT-NOTE: sge_readpid() is MT safe.
 */
pid_t sge_readpid(const char *fname) {
   DENTER(TOP_LAYER);

   FILE *fp;
   char buf[512], *cp;
   pid_t pid;

   if (!(fp = fopen(fname, "r"))) {
      DRETURN(0);
   }

   pid = 0;
   while (fgets(buf, sizeof(buf), fp)) {
      char *pos = nullptr;

      /*
       * set chrptr to the first non blank character
       * If line is empty continue with next line
       */
      if (!(cp = strtok_r(buf, " \t\n", &pos))) {
         continue;
      }

      /* Check for negative numbers */
      if (!isdigit((int) *cp)) {
         pid = 0;
      } else {
         pid = atoi(cp);
      }
      break;
   }

   FCLOSE(fp);

   DRETURN(pid);
   FCLOSE_ERROR:
DRETURN(0);
} /* sge_readpid() */

/**
 * @brief Write pid into file
 *
 * Write pid into file
 *
 * @param pid_log_file filename
 *
 * @note MT-NOTE: sge_write_pid() is MT safe
 */
void sge_write_pid(const char *pid_log_file) {
   DENTER(TOP_LAYER);

   int pid;
   FILE *fp;

   close(creat(pid_log_file, 0644));
   if ((fp = fopen(pid_log_file, "w")) != nullptr) {
      pid = getpid();
      FPRINTF((fp, "%d\n", pid));
      FCLOSE(fp);
   }
   DRETURN_VOID;
   FPRINTF_ERROR:
   FCLOSE_ERROR:
   /* EB: TODO: CLEANUP: make it possible that calling function handles this error */
DRETURN_VOID;
}


/**
 * @brief Get config value for
 *
 * Get config value for entry 'conf_val' from file 'fname'.
 *
 * @param conf_val is case insensitive name
 * @param fname filename
 *
 * @return pointer to internal static buffer
 *
 * @note Lines may be up to 1024 characters long. Up to 1024 characters of the
 *       config value are copied to the static buffer.
 *
 *       MT-NOTE: sge_get_confval() is MT safe
 */
char *sge_get_confval(const char *conf_val, const char *fname) {
   static char valuev[1][4097];
   bootstrap_entry_t namev[1];

   namev[0].name = conf_val;
   namev[0].is_required = true;
   if (sge_get_confval_array(fname, 1, 1, namev, valuev, nullptr)) {
      return nullptr;
   } else {
      return valuev[0];
   }
}

/**
 * @brief Reads in an array of configuration file entries
 *
 * Reads in an array of configuration file entries
 *
 * @return 0 on success
 *
 * @note MT-NOTE: sge_get_confval_array() is MT safe
 *
 * @bug Function can not differ multiple similar named entries.
 *
 * @param fname path of the bootstrap file to read
 * @param n number of entries in @p name and @p value
 * @param nmissing how many of the entries may be absent without it being an error
 * @param name the keys to look for, see #bootstrap_entry_t
 * @param value receives the value of each key, in the order of @p name
 * @param error_dstring if not nullptr, an error message is appended here
*/
int sge_get_confval_array(const char *fname, int n, int nmissing, bootstrap_entry_t name[],
                          char value[][4097], dstring *error_dstring) {
   DENTER(TOP_LAYER);

   FILE *fp;
   char buf[4096], *cp;
   int i;
   bool *is_found = nullptr;

   if (!(fp = fopen(fname, "r"))) {
      if (error_dstring == nullptr) {
         CRITICAL(MSG_FILE_FOPENFAILED_SS, fname, strerror(errno));
      } else {
         sge_dstring_sprintf(error_dstring, MSG_FILE_FOPENFAILED_SS,
                             fname, strerror(errno));
      }
      DRETURN(n);
   }
   is_found = reinterpret_cast<bool *>(sge_malloc(sizeof(bool) * n));
   SGE_ASSERT(is_found != nullptr);
   memset(is_found, false, n * sizeof(bool));

   while (fgets(buf, sizeof(buf), fp)) {
      char *pos = nullptr;

      /* set chrptr to the first non-blank character
       * If line is empty continue with next line
       */
      if (!(cp = strtok_r(buf, " \t\n", &pos))) {
         continue;
      }

      /* allow commentaries */
      if (cp[0] == '#') {
         continue;
      }

      /* search for all requested configuration values */
      for (i = 0; i < n; i++) {
         if (strcasecmp(name[i].name, cp) == 0) {
            /*
             * Take the rest of the line as the value. We cannot tokenize on
             * whitespace here: the postgres `spooling_params` line is a libpq
             * conninfo string that carries spaces between its key=value pairs
             * (host=... port=... dbname=...), and tokenizing would silently drop
             * everything after the first key=value. The classic and berkeleydb
             * spool_params values are single tokens, so they used to be parsed
             * correctly by strtok_r; the new logic preserves that case and
             * additionally handles multi-token values cleanly.
             *
             * pos points just past the NUL that strtok_r wrote in place of the
             * delimiter that ended the attribute name. Skip remaining
             * whitespace, then copy through end-of-line, stripping trailing
             * whitespace and the line terminator.
             */
            if (pos != nullptr) {
               while (*pos == ' ' || *pos == '\t') {
                  pos++;
               }
               if (*pos != '\0' && *pos != '\n' && *pos != '\r') {
                  char *value_end = pos;
                  while (*value_end != '\0' && *value_end != '\n' && *value_end != '\r') {
                     value_end++;
                  }
                  while (value_end > pos &&
                         (value_end[-1] == ' ' || value_end[-1] == '\t')) {
                     value_end--;
                  }
                  size_t len = value_end - pos;
                  if (len > 4096) {
                     len = 4096;
                  }
                  memcpy(value[i], pos, len);
                  value[i][len] = '\0';
                  cp = value[i];
                  is_found[i] = true;
                  if (name[i].is_required) {
                     --nmissing;
                  }
               }
            }
            break;
         }
      }
   }
   if (nmissing != 0) {
      for (i = 0; i < n; i++) {
         if (!is_found[i]) {
            *value[i] = '\0';
            if (name[i].is_required) {
               if (error_dstring == nullptr) {
                  CRITICAL(MSG_UTI_CANNOTLOCATEATTRIBUTE_SS, name[i].name, fname);
               } else {
                  sge_dstring_sprintf(error_dstring, MSG_UTI_CANNOTLOCATEATTRIBUTE_SS,
                                      name[i].name, fname);
               }

               break;
            }
         }
      }
   }

   sge_free(&is_found);
   FCLOSE(fp);
   DRETURN(nmissing);
   FCLOSE_ERROR:
   DRETURN(0);
} /* sge_get_confval_array() */



/**
 * @brief Set display mode
 *
 * With 'STATUS_ROTATING_BAR' each call of
 * sge_status_next_turn() will show a rotating bar.
 * In 'STATUS_DOTS'-mode each call will show more
 * dots in a line.
 *
 * @param type display type STATUS_ROTATING_BAR STATUS_DOTS
 *
 * @note MT-NOTE: sge_status_set_type() is not MT safe
 */
void sge_status_set_type(washing_machine_t type) {
   wtype = type;
}

/**
 * @brief Show next turn
 *
 * Show next turn of rotating washing machine.
 *
 * @note MT-NOTE: sge_status_next_turn() is not MT safe
 */
void sge_status_next_turn() {
   static int cnt = 0;
   static const char s[] = "-\\/";
   static const char *sp = nullptr;

   cnt++;
   if ((cnt % 100) != 1) {
      return;
   }

   switch (wtype) {
      case STATUS_ROTATING_BAR: {

         if (!sge_silent_get()) {
            if (!sp || !*sp) {
               sp = s;
            }
            printf("%c\b", *sp++);
            fflush(stdout);
         }
      }
         break;
      case STATUS_DOTS:
         if (!sge_silent_get()) {
            printf(".");
            fflush(stdout);
         }
         break;
      default:
         break;
   }
}

/**
 * @brief Remove washing machine from display
 *
 * Last turn of washing machine.
 *
 * @note MT-NOTE: sge_status_end_turn() is not MT safe
 */
void sge_status_end_turn() {
   switch (wtype) {
      case STATUS_ROTATING_BAR:
         if (!sge_silent_get()) {
            printf(" \b");
            fflush(stdout);
         }
         break;
      case STATUS_DOTS:
         if (!sge_silent_get()) {
            printf("\n");
            fflush(stdout);
         }
         break;
      default:
         break;
   }
}

/**
 * @brief Enable/disable silence during spool ops
 *
 * Enable/disable silence during spool operations. Silence means
 * that no messages are printed to stdout.
 *
 * @param i 0 or 1
 *
 * @note MT-NOTE: sge_silent_set() is not MT safe
 *
 * @see #sge_silent_get
 */
void sge_silent_set(int i) {
   silent_flag = i;
}

/**
 * @brief Show whether silence is enable/disabled
 *
 * Show whether silence is enable/disabled
 *
 * @return 0 or 1
 *
 * @note MT-NOTE: sge_silent_get() is not MT safe
 *
 * @see #sge_silent_set
 */
int sge_silent_get() {
   return silent_flag;
}

/**
 * @brief Reads in an array of configuration file entries
 *
 * Reads in an array of configuration file entries
 *
 * @return 0 on success
 *
 * @note MT-NOTE: sge_get_management_entry() is MT safe
 *
 * @bug Function can not differ multiple similar named entries.
 *
 * @param fname path of the management file to read
 * @param n number of entries in @p name and @p value
 * @param nmissing how many of the entries may be absent without it being an error
 * @param name the keys to look for, see #bootstrap_entry_t
 * @param value receives the value of each key, in the order of @p name
 * @param error_dstring if not nullptr, an error message is appended here
*/
int sge_get_management_entry(const char *fname, int n, int nmissing, bootstrap_entry_t name[],
                             char value[][SGE_PATH_MAX], dstring *error_dstring) {
   DENTER(TOP_LAYER);

   FILE *fp;
   char buf[SGE_PATH_MAX], *cp;
   int i;
   bool *is_found = nullptr;

   if (!(fp = fopen(fname, "r"))) {
      if (error_dstring == nullptr) {
         CRITICAL(MSG_FILE_FOPENFAILED_SS, fname, strerror(errno));
      } else {
         sge_dstring_sprintf(error_dstring, MSG_FILE_FOPENFAILED_SS,
                             fname, strerror(errno));
      }
      DRETURN(n);
   }
   is_found = (bool *) sge_malloc(sizeof(bool) * n);
   SGE_ASSERT(is_found != nullptr);
   memset(is_found, false, n * sizeof(bool));

   while (fgets(buf, sizeof(buf), fp)) {
      char *pos = nullptr;

      /* set chrptr to the first non blank character
       * If line is empty continue with next line
       */
      if (!(cp = strtok_r(buf, " \t\n", &pos))) {
         continue;
      }

      /* allow commentaries */
      if (cp[0] == '#') {
         continue;
      }

      /* search for all requested configuration values */
      for (i = 0; i < n; i++) {
         char *nam = strtok_r(cp, "=", &pos);
         char *val = strtok_r(nullptr, "\n", &pos);
         if (nam != nullptr && strcasecmp(name[i].name, nam) == 0) {
            DPRINTF("nam = %s\n", nam);
            if (val != nullptr) {
               DPRINTF("val = %s\n", val);
               sge_strlcpy(value[i], val, SGE_PATH_MAX);
            } else {
               sge_strlcpy(value[i], "", SGE_PATH_MAX);
            }
            is_found[i] = true;
            if (name[i].is_required) {
               --nmissing;
            }
            break;
         }
      }
   }
   if (nmissing != 0) {
      for (i = 0; i < n; i++) {
         if (!is_found[i] && name[i].is_required) {
            if (error_dstring == nullptr) {
               CRITICAL(MSG_UTI_CANNOTLOCATEATTRIBUTEMAN_SS, name[i].name, fname);
            } else {
               sge_dstring_sprintf(error_dstring, MSG_UTI_CANNOTLOCATEATTRIBUTEMAN_SS,
                                   name[i].name, fname);
            }

            break;
         }
      }
   }

   sge_free(&is_found);
   FCLOSE(fp);
   DRETURN(nmissing);
   FCLOSE_ERROR:
DRETURN(0);
} /* sge_get_management_entry() */

/**
 * @brief Create paths in active_jobs dir
 *
 * Creates paths in the execd's active_jobs directory.
 * Both directory and file paths can be created.
 * The result is placed in a buffer provided by the caller.
 *
 * WARNING: Do only use in shepherd and execution daemon!
 *
 * @code
 * To create the relative path to a jobs/tasks environment file, the
 * following call would be used:
 *
 * char buffer[SGE_PATH_MAX]
 * sge_get_active_job_file_path(buffer, SGE_PATH_MAX,
 *                              job_id, ja_task_id, pe_task_id,
 *                              "environment");
 * @endcode
 *
 * @param buffer buffer to hold the generated path
 * @param job_id job id
 * @param ja_task_id array task id
 * @param pe_task_id optional pe task id
 * @param filename optional file name
 *
 * @return pointer to the string buffer on success, else nullptr
 *
 * @note JG: TODO: The function might be converted to or might use a more
 *       general path creating function (utilib).
 *
 * @see `sge_make_ja_task_active_dir()`, `sge_make_pe_task_active_dir()`
 */
const char *sge_get_active_job_file_path(dstring *buffer, uint32_t job_id,
                                         uint32_t ja_task_id, const char *pe_task_id, const char *filename) {
   DENTER(TOP_LAYER);

   if (buffer == nullptr) {
      DRETURN(nullptr);
   }

   sge_dstring_sprintf(buffer, "%s/" sge_u32"." sge_u32, ACTIVE_DIR, job_id, ja_task_id);

   if (pe_task_id != nullptr) {
      sge_dstring_append_char(buffer, '/');
      sge_dstring_append(buffer, pe_task_id);
   }

   if (filename != nullptr) {
      sge_dstring_append_char(buffer, '/');
      sge_dstring_append(buffer, filename);
   }

   DRETURN(sge_dstring_get_string(buffer));
}


