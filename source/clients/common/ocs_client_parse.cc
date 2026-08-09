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
 *  Portions of this software are Copyright (c) 2023-2024,2026 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

/** @file
 * @brief Command line handling every client shares: option parsing and the standard error paths
 */
#include <pwd.h>
#include <cstring>
#include <unistd.h>

#include "uti/sge_io.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"
#include "uti/sge_dstring.h"
#include "uti/sge_string.h"
#include "uti/sge_stdlib.h"

#include "sgeobj/parse.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"

#include "msg_clients_common.h"
#include "msg_common.h"
#include "ocs_client_parse.h"
#include "parse_job_cull.h"
#include "parse_qsub.h"
#include "sge_options.h"

static char *get_cwd_defaults_file_path(lList **answer_list);
static void append_opts_from_default_files(uint32_t prog_number, lList **pcmdline, lList **answer_list, char **envp,
                                           char **def_files);

/**
 * @brief TODO document this
 */
void opt_list_append_opts_from_default_files(uint32_t prog_number, const char *cell_root, const char *user,
                                             lList **pcmdline, lList **answer_list, char **envp) {
   DENTER(TOP_LAYER);

   lFreeList(answer_list);

   /* the sge root defaults file */
   dstring req_file = DSTRING_INIT;
   get_root_file_path(&req_file, cell_root, SGE_COMMON_DEF_REQ_FILE);
   char *def_files[3 + 1];
   def_files[0] = strdup(sge_dstring_get_string(&req_file));

   /*
    * the defaults file in the user's home directory
    */
   get_user_home_file_path(&req_file, SGE_HOME_DEF_REQ_FILE, user, answer_list);
   def_files[1] = strdup(sge_dstring_get_string(&req_file));

   /*
    * the defaults file in the current working directory
    */
   def_files[2] = get_cwd_defaults_file_path(answer_list);

   def_files[3] = nullptr;

   /*
    * now read all the defaults files, unaware of where they came from
    */
   append_opts_from_default_files(prog_number, pcmdline, answer_list, envp, def_files); /* MT-NOTE !!!! */

   sge_dstring_free(&req_file);

   DRETURN_VOID;
}

/**
 * @brief TODO document this
 */
bool get_user_home_file_path(dstring *absolut_filename, const char *filename, const char *user, lList **answer_list) {
   DENTER(TOP_LAYER);

   if (absolut_filename == nullptr || filename == nullptr) {
      DRETURN(false);
   }

   sge_dstring_clear(absolut_filename);

   if (get_user_home(absolut_filename, user, answer_list)) {
      sge_dstring_append(absolut_filename, "/");
      sge_dstring_append(absolut_filename, filename);
      DRETURN(true);
   }

   DRETURN(false);
}

/**
 * @brief TODO document this
 */
static char *get_cwd_defaults_file_path(lList **answer_list) {
   DENTER(TOP_LAYER);
   char cwd[SGE_PATH_MAX + 1];
   if (!getcwd(cwd, sizeof(cwd))) {
      char str[MAX_STRING_SIZE];
      snprintf(str, sizeof(str), SFNMAX, MSG_FILE_CANTREADCURRENTWORKINGDIR);
      answer_list_add(answer_list, str, STATUS_EDISK, ANSWER_QUALITY_ERROR);
      DRETURN(nullptr);
   }

   char *file = sge_malloc(strlen(cwd) + strlen(SGE_HOME_DEF_REQ_FILE) + 2);

   strcpy(file, cwd);
   if (*file && (file[strlen(file) - 1] != '/')) {
      strcat(file, "/");
   }
   strcat(file, SGE_HOME_DEF_REQ_FILE);
   DRETURN(file);
}

/**
 * @brief TODO document this
 */
static void append_opts_from_default_files(uint32_t prog_number, lList **pcmdline, lList **answer_list, char **envp,
                                           char **def_files) {
   DENTER(TOP_LAYER);
   char **pstr;
   SGE_STRUCT_STAT buf{};
   int do_exit = 0;

   for (pstr = def_files; *pstr; pstr++) {
      if (SGE_STAT(*pstr, &buf) < 0) {
         DPRINTF("-- defaults file %s does not exist\n", *pstr);
         continue;
      }

      int already_read = 0;
      for (char **ppstr = def_files; *ppstr != *pstr; ppstr++) {
         if (!sge_filecmp(*ppstr, *pstr)) {
            DPRINTF("-- skipping %s as defaults file - already read as %s\n", *pstr, *ppstr);
            already_read = 1;
            break;
         }
      }
      if (already_read) {
         continue;
      }
      DPRINTF("-- defaults file: %s\n", *pstr);

      lList *alp = parse_script_file(prog_number, *pstr, "", pcmdline, envp, FLG_HIGHER_PRIOR | FLG_USE_NO_PSEUDOS);

      for_each_ep_lv(aep, alp) {
         uint32_t status;
         answer_quality_t quality;

         status = lGetUlong(aep, AN_status);
         quality = (answer_quality_t)lGetUlong(aep, AN_quality);

         if (quality == ANSWER_QUALITY_ERROR) {
            DPRINTF("%s", lGetString(aep, AN_text));
            if (status == STATUS_EDISK) {
               /*
               ** we turn this error into a warning here
               */
               quality = ANSWER_QUALITY_WARNING;
            } else {
               do_exit = 1;
            }
         } else {
            DPRINTF("Warning: Error: %s\n", lGetString(aep, AN_text));
         }
         answer_list_add(answer_list, lGetString(aep, AN_text), status, quality);
      }
      lFreeList(&alp);

      if (do_exit) {
         for (pstr = def_files; *pstr; pstr++) {
            sge_free(pstr);
         }
         DRETURN_VOID;
      }
   }

   for (pstr = def_files; *pstr; pstr++) {
      sge_free(pstr);
   }

   DRETURN_VOID;
}
/**
 * @brief TODO document this
 */
void opt_list_append_opts_from_qsub_cmdline(uint32_t prog_number, lList **opts_cmdline, lList **answer_list,
                                            const char **argv, char **envp) {
   lFreeList(answer_list);
   *answer_list = cull_parse_cmdline(prog_number, argv, envp, opts_cmdline, FLG_USE_PSEUDOS);
}

/**
 * @brief TODO document this
 */
void opt_list_append_opts_from_qalter_cmdline(uint32_t prog_number, lList **opts_cmdline, lList **answer_list,
                                              const char **argv, char **envp) {
   lFreeList(answer_list);
   *answer_list = cull_parse_cmdline(prog_number, argv, envp, opts_cmdline, FLG_USE_PSEUDOS | FLG_QALTER);
}

/**
 * @brief TODO document this
 */
void opt_list_append_opts_from_script(uint32_t prog_number, lList **opts_scriptfile, lList **answer_list,
                                      const lList *opts_cmdline, char **envp) {
   const char *scriptfile = nullptr;
   const lListElem *script_option = lGetElemStr(opts_cmdline, SPA_switch_val, STR_PSEUDO_SCRIPT);
   if (script_option != nullptr) {
      scriptfile = lGetString(script_option, SPA_argval_lStringT);
   }
   const lListElem *c_option = lGetElemStr(opts_cmdline, SPA_switch_val, "-C");
   const char *prefix = nullptr;
   if (c_option != nullptr) {
      prefix = lGetString(c_option, SPA_argval_lStringT);
   } else {
      prefix = default_prefix;
   }
   lFreeList(answer_list);
   *answer_list = parse_script_file(prog_number, scriptfile, prefix, opts_scriptfile, envp, FLG_DONT_ADD_SCRIPT);
}

/**
 * @brief TODO document this
 */
void opt_list_append_opts_from_script_path(uint32_t prog_number, lList **opts_scriptfile, const char *path,
                                           lList **answer_list, const lList *opts_cmdline, char **envp) {
   char *scriptpath = nullptr;

   const lListElem *script_option = lGetElemStr(opts_cmdline, SPA_switch_val, STR_PSEUDO_SCRIPT);
   if (script_option != nullptr) {
      const char *scriptfile = lGetString(script_option, SPA_argval_lStringT);

      /* If the scriptfile path isn't absolute (which includes starting with
         $HOME), make it absolute relative to the given path.
         If the script or path is nullptr, let parse_script_file() catch it. */
      if ((scriptfile != nullptr) && (path != nullptr) && (scriptfile[0] != '/') && (strncmp(scriptfile, "$HOME/", 6) != 0) &&
          (strcmp(scriptfile, "$HOME") != 0)) {
         /* Malloc space for the path, the filename, the \0, and perhaps a / */
         const size_t size = strlen(path) + strlen(scriptfile) + 2;
         scriptpath = sge_malloc(sizeof(char) * size);
         const char *sep = (path[strlen(path) - 1] != '/') ? "/" : "";
         snprintf(scriptpath, size, "%s%s%s", path, sep, scriptfile);
      } else if (scriptfile) {
         scriptpath = strdup(scriptfile);
      }
   }

   const char *prefix = nullptr;
   if (const lListElem *c_option = lGetElemStr(opts_cmdline, SPA_switch_val, "-C"); c_option != nullptr) {
      prefix = lGetString(c_option, SPA_argval_lStringT);
   } else {
      prefix = default_prefix;
   }

   lFreeList(answer_list);

   *answer_list = parse_script_file(prog_number, scriptpath, prefix, opts_scriptfile, envp, FLG_DONT_ADD_SCRIPT);

   sge_free(&scriptpath);
}

/**
 * @brief TODO document this
 */
void opt_list_merge_command_lines(lList **opts_all, lList **opts_defaults, lList **opts_scriptfile,
                                  lList **opts_cmdline) {
   // Order is very important here
   if (*opts_defaults != nullptr) {
      if (*opts_all == nullptr) {
         *opts_all = *opts_defaults;
      } else {
         lAddList(*opts_all, opts_defaults);
      }
      *opts_defaults = nullptr;
   }
   if (*opts_scriptfile != nullptr) {
      if (*opts_all == nullptr) {
         *opts_all = *opts_scriptfile;
      } else {
         /* Override the queue (-q) values from defaults */
         lOverrideStrList(*opts_all, *opts_scriptfile, SPA_switch_val, "-q");
      }
      *opts_scriptfile = nullptr;
   }
   if (*opts_cmdline != nullptr) {
      if (*opts_all == nullptr) {
         *opts_all = *opts_cmdline;
      } else {
         /* Override queue (-q) values from both defaults and scriptfile */
         lOverrideStrList(*opts_all, *opts_cmdline, SPA_switch_val, "-q");
      }
      *opts_cmdline = nullptr;
   }

   /* If -ar was requested add -w if it was not explicitly set */
   if (lGetElemStr(*opts_all, SPA_switch_val, "-ar") != nullptr) {
      if (lGetElemStr(*opts_all, SPA_switch_val, "-w") == nullptr) {
         lListElem *ep_opt = sge_add_arg(opts_all, r_OPT, lIntT, "-w", "e");
         lSetInt(ep_opt, SPA_argval_lIntT, ERROR_VERIFY);
      }
   }
}

/**
 * @brief TODO document this
 */
bool opt_list_has_X(lList *opts, const char *option) {
   bool ret = false;

   const lListElem *opt = lGetElemStr(opts, SPA_switch_val, option);
   if (opt != nullptr) {
      ret = true;
   }
   return ret;
}

/**
 * @brief TODO document this
 */
bool opt_list_is_X_true(lList *opts, const char *option) {
   bool ret = false;

   const lListElem *opt = lGetElemStr(opts, SPA_switch_val, option);
   if (opt != nullptr) {
      ret = (lGetInt(opt, SPA_argval_lIntT) == 1) ? true : false;
   }
   return ret;
}

void opt_list_verify_scope(lList *opts, lList **alpp) {
   if (lGetElemStr(opts, SPA_switch_val, "-masterq") != nullptr &&
       lGetElemStr(opts, SPA_switch_val, "-scope") != nullptr) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_EITHERSCOPEORMASTERX);
   }
}

/**
 * @brief TODO document this
 */
const char *get_root_file_path(dstring *absolut_filename, const char *cell_root, const char *filename) {
   DENTER(TOP_LAYER);
   sge_dstring_sprintf(absolut_filename, "%s/%s", cell_root, filename);
   DRETURN(sge_dstring_get_string(absolut_filename));
}

/**
 * @brief TODO document this
 */
bool get_user_home(dstring *home_dir, const char *user, lList **answer_list) {
   DENTER(TOP_LAYER);

   if (home_dir == nullptr) {
      DRETURN(false);
   }

   passwd pw_struct{};
   const int size = get_pw_buffer_size();
   char *buffer = sge_malloc(size);
   const passwd *pwd = sge_getpwnam_r(user, &pw_struct, buffer, size);
   if (pwd == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ENOSUCHUSER, ANSWER_QUALITY_ERROR, MSG_USER_INVALIDNAMEX_S, user);
      DRETURN(false);
   }
   if (pwd->pw_dir == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EDISK, ANSWER_QUALITY_ERROR, MSG_USER_NOHOMEDIRFORUSERX_S, user);
      DRETURN(false);
   }

   sge_dstring_copy_string(home_dir, pwd->pw_dir);
   sge_free(&buffer);
   DRETURN(true);
}
