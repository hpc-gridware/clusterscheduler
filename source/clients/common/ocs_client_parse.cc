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
#include "sgeobj/tom/ocs_tom_SPA.h"
#include "tom/ocs_Element.h"

#include "msg_clients_common.h"
#include "msg_common.h"
#include "ocs_client_parse.h"
#include "parse_job_cull.h"
#include "parse_qsub.h"
#include "sge_options.h"

static char *get_cwd_defaults_file_path(ocs::tom::ListPtr<ocs::tom::AN> *answer_list);
static void append_opts_from_default_files(uint32_t prog_number, ocs::tom::ListPtr<ocs::tom::SPA> &cmdline,
                                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list, char **envp, char **def_files);

/**
 * @brief Parse default files
 *
 * This function reads the 3 defaults files if they exist and parses them
 * into an options list.
 *
 * @param prog_number the client reading the defaults, which decides which files are read
 * @param cell_root the cell directory, where the cluster-wide file lives
 * @param user the user whose home directory holds the second file
 * @param cmdline the option list (`SPA_Type`) to append to; created if it is
 *                an empty handle and the files hold any options
 * @param answer_list receives warnings; none of these stops the parse:
 *                    `STATUS_ENOSUCHUSER` (no passwd entry for the user),
 *                    `STATUS_EDISK` (home or current directory unreadable, or
 *                    the file could not be opened), `STATUS_EEXIST` and
 *                    `STATUS_EUNKNOWN` (from #parse_script_file, which may
 *                    return others as well)
 * @param envp environment pointer
 *
 * @note MT-NOTE: opt_list_append_opts_from_default_files() is MT safe
 */
void opt_list_append_opts_from_default_files(const uint32_t prog_number, const char *cell_root, const char *user,
                                             ocs::tom::ListPtr<ocs::tom::SPA> &cmdline,
                                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list, char **envp) {
   DENTER(TOP_LAYER);

   if (answer_list != nullptr) {
      answer_list->reset();
   }

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
   append_opts_from_default_files(prog_number, cmdline, answer_list, envp, def_files); /* MT-NOTE !!!! */

   sge_dstring_free(&req_file);

   DRETURN_VOID;
}

/**
 * @brief opt_list_append_opts_from_default_files() for an untyped caller
 *
 * @param prog_number the client reading the defaults, which decides which files are read
 * @param cell_root the cell directory, where the cluster-wide file lives
 * @param user the user whose home directory holds the second file
 * @param pcmdline the option list (`SPA_Type`) to append to, checked; created if it is
 *                 `nullptr` and the files hold any options
 * @param answer_list receives warnings, as the typed form writes them
 * @param envp environment pointer
 */
void opt_list_append_opts_from_default_files(const uint32_t prog_number, const char *cell_root, const char *user,
                                             lList **pcmdline, ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                             char **envp) {
   ocs::tom::ListPtr<ocs::tom::SPA> cmdline(*pcmdline);
   opt_list_append_opts_from_default_files(prog_number, cell_root, user, cmdline, answer_list, envp);
   *pcmdline = cmdline.release();
}

/**
 * @brief Get absolut path name to file in user
 *
 * This function returns the path to the file in the user's home
 * directory
 *
 * @param absolut_filename receives the computed absolute file name
 * @param filename file name
 * @param user user name
 * @param answer_list receives warnings; none of these stops the parse:
 *                    `STATUS_ENOSUCHUSER` (no passwd entry for the user),
 *                    `STATUS_EDISK` (home or current directory unreadable, or
 *                    the file could not be opened), `STATUS_EEXIST` and
 *                    `STATUS_EUNKNOWN` (from #parse_script_file, which may
 *                    return others as well)
 *
 * @return true or false
 *
 * @note MT-NOTE: get_user_home_file_path() is MT safe
 */
bool get_user_home_file_path(dstring *absolut_filename, const char *filename, const char *user,
                             ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
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
 * @brief Find cwd default file path
 *
 * This function returns the path of the defaults file in the current working
 * directory
 *
 * @param answer_list receives warnings; none of these stops the parse:
 *                    `STATUS_ENOSUCHUSER` (no passwd entry for the user),
 *                    `STATUS_EDISK` (home or current directory unreadable, or
 *                    the file could not be opened), `STATUS_EEXIST` and
 *                    `STATUS_EUNKNOWN` (from #parse_script_file, which may
 *                    return others as well)
 */
static char *get_cwd_defaults_file_path(ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
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
 * @brief Parse default files
 *
 * This function reads the defaults files pointed to by def_files[] if they
 * exist and parses them into an options list.
 *
 * @param prog_number the client reading the defaults, which decides which files are read
 * @param cell_root the cell directory, where the cluster-wide file lives
 * @param user the user whose home directory holds the second file
 * @param cmdline the option list (`SPA_Type`) to append to; created if it is
 *                an empty handle and the files hold any options
 * @param answer_list receives warnings; none of these stops the parse:
 *                    `STATUS_ENOSUCHUSER` (no passwd entry for the user),
 *                    `STATUS_EDISK` (home or current directory unreadable, or
 *                    the file could not be opened), `STATUS_EEXIST` and
 *                    `STATUS_EUNKNOWN` (from #parse_script_file, which may
 *                    return others as well)
 * @param envp environment pointer
 * @param def_files paths to default files
 */
static void append_opts_from_default_files(const uint32_t prog_number, ocs::tom::ListPtr<ocs::tom::SPA> &cmdline,
                                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list, char **envp,
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

      // parse_script_file() has no typed form yet; it appends to (or creates) the raw list
      lList *cmdline_lp = cmdline.release();
      auto alp = parse_script_file(prog_number, *pstr, "", &cmdline_lp, envp, FLG_HIGHER_PRIOR | FLG_USE_NO_PSEUDOS);
      cmdline = ocs::tom::ListPtr<ocs::tom::SPA>::from_typed(cmdline_lp);

      for_each_ep_lv(aep, alp) {
         uint32_t status;
         answer_quality_t quality;

         status = ocs::tom::get<AN_status>(aep);
         quality = (answer_quality_t)ocs::tom::get<AN_quality>(aep);

         if (quality == ANSWER_QUALITY_ERROR) {
            DPRINTF("%s", ocs::tom::get<AN_text>(aep));
            if (status == STATUS_EDISK) {
               /*
               ** we turn this error into a warning here
               */
               quality = ANSWER_QUALITY_WARNING;
            } else {
               do_exit = 1;
            }
         } else {
            DPRINTF("Warning: Error: %s\n", ocs::tom::get<AN_text>(aep));
         }
         answer_list_add(answer_list, ocs::tom::get<AN_text>(aep), status, quality);
      }
      alp.reset();

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
 * @brief Parse opts from cmd line
 *
 * Parse options from the qsub commandline given by "argv" and store
 * the parsed objects in "opts_cmdline". If an error occures store
 * the error/warning messages in the "answer_list".
 * "envp" is a pointer to the process environment.
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param opts_cmdline command line options (`SPA_Type`); created if it is an empty handle
 * @param answer_list AN_Type list
 * @param argv Argumente
 * @param envp Environment
 */
void opt_list_append_opts_from_qsub_cmdline(const uint32_t prog_number, ocs::tom::ListPtr<ocs::tom::SPA> &opts_cmdline,
                                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const char **argv,
                                            char **envp) {
   DENTER(TOP_LAYER);
   if (answer_list != nullptr) {
      answer_list->reset();
   }
   // cull_parse_cmdline() has no typed form yet; it appends to (or creates) the raw list
   lList *cmdline_lp = opts_cmdline.release();
   *answer_list = cull_parse_cmdline(prog_number, argv, envp, &cmdline_lp, FLG_USE_PSEUDOS);
   opts_cmdline = ocs::tom::ListPtr<ocs::tom::SPA>::from_typed(cmdline_lp);
   DRETURN_VOID;
}

/**
 * @brief opt_list_append_opts_from_qsub_cmdline() for an untyped caller
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param opts_cmdline command line options (`SPA_Type`), checked; created if it is `nullptr`
 * @param answer_list AN_Type list
 * @param argv Argumente
 * @param envp Environment
 */
void opt_list_append_opts_from_qsub_cmdline(const uint32_t prog_number, lList **opts_cmdline,
                                            ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                            const char **argv, char **envp) {
   ocs::tom::ListPtr<ocs::tom::SPA> cmdline(*opts_cmdline);
   opt_list_append_opts_from_qsub_cmdline(prog_number, cmdline, answer_list, argv, envp);
   *opts_cmdline = cmdline.release();
}

/**
 * @brief Parse opts from cmd line
 *
 * Parse options from the qalter commandline given by "argv" and store
 * the parsed objects in "opts_cmdline". If an error occures store
 * the error/warning messages in the "answer_list".
 * "envp" is a pointer to the process environment.
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param opts_cmdline command line options (`SPA_Type`); created if it is an empty handle
 * @param answer_list AN_Type list
 * @param argv Argumente
 * @param envp Environment
 */
void opt_list_append_opts_from_qalter_cmdline(const uint32_t prog_number,
                                              ocs::tom::ListPtr<ocs::tom::SPA> &opts_cmdline,
                                              ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                              const char **argv, char **envp) {
   DENTER(TOP_LAYER);
   if (answer_list != nullptr) {
      answer_list->reset();
   }
   // cull_parse_cmdline() has no typed form yet; it appends to (or creates) the raw list
   lList *cmdline_lp = opts_cmdline.release();
   *answer_list = cull_parse_cmdline(prog_number, argv, envp, &cmdline_lp, FLG_USE_PSEUDOS | FLG_QALTER);
   opts_cmdline = ocs::tom::ListPtr<ocs::tom::SPA>::from_typed(cmdline_lp);
   DRETURN_VOID;
}

/**
 * @brief Parse opts from scriptfile
 *
 * This function parses the commandline options which are embedded
 * in scriptfile (jobscript) and stores the parsed objects in
 * opts_scriptfile. The filename of the scriptfile has to be
 * contained in the list "opts_cmdline" which has been previously i
 * created with opt_list_append_opts_from_*_cmdline(). "answer_list"
 * will be used to store error/warning messages.
 * "envp" is a pointer to the process environment.
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param opts_scriptfile embedded command line options (`SPA_Type`); created if it is an empty handle
 * @param answer_list AN_Type list
 * @param opts_cmdline Argumente
 * @param envp Environment
 */
void opt_list_append_opts_from_script(const uint32_t prog_number, ocs::tom::ListPtr<ocs::tom::SPA> &opts_scriptfile,
                                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                      const ocs::tom::ConstList<ocs::tom::SPA> opts_cmdline, char **envp) {
   DENTER(TOP_LAYER);
   const char *scriptfile = nullptr;
   if (const auto script_option = opts_cmdline.find_first<SPA_switch_val>(STR_PSEUDO_SCRIPT)) {
      scriptfile = ocs::tom::get<SPA_argval_lStringT>(script_option);
   }
   const char *prefix = nullptr;
   if (const auto c_option = opts_cmdline.find_first<SPA_switch_val>("-C")) {
      prefix = ocs::tom::get<SPA_argval_lStringT>(c_option);
   } else {
      prefix = default_prefix;
   }
   if (answer_list != nullptr) {
      answer_list->reset();
   }
   // parse_script_file() has no typed form yet; it appends to (or creates) the raw list
   lList *scriptfile_lp = opts_scriptfile.release();
   *answer_list = parse_script_file(prog_number, scriptfile, prefix, &scriptfile_lp, envp, FLG_DONT_ADD_SCRIPT);
   opts_scriptfile = ocs::tom::ListPtr<ocs::tom::SPA>::from_typed(scriptfile_lp);
   DRETURN_VOID;
}

/**
 * @brief opt_list_append_opts_from_script() for an untyped caller
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param opts_scriptfile embedded command line options (`SPA_Type`), checked; created if it is `nullptr`
 * @param answer_list AN_Type list
 * @param opts_cmdline Argumente (`SPA_Type`), checked
 * @param envp Environment
 */
void opt_list_append_opts_from_script(const uint32_t prog_number, lList **opts_scriptfile,
                                      ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                      const lList *opts_cmdline, char **envp) {
   ocs::tom::ListPtr<ocs::tom::SPA> scriptfile(*opts_scriptfile);
   opt_list_append_opts_from_script(prog_number, scriptfile, answer_list,
                                    ocs::tom::ConstList<ocs::tom::SPA>(opts_cmdline), envp);
   *opts_scriptfile = scriptfile.release();
}

/**
 * @brief Parse opts from scriptfile
 *
 * This function parses the commandline options which are embedded
 * in scriptfile (jobscript) and stores the parsed objects in
 * opts_scriptfile. The filename of the scriptfile has to be
 * contained in the list "opts_cmdline" which has been previously i
 * created with opt_list_append_opts_from_*_cmdline(). If the filename of
 * the scriptfile is not an absolute path, "path" will be prepended to it.
 * "answer_list" will be used to store error/warning messages.
 * "envp" is a pointer to the process environment.
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param opts_scriptfile embedded command line options (`SPA_Type`); created if it is an empty handle
 * @param path the root path for the script file
 * @param answer_list AN_Type list
 * @param opts_cmdline Argumente
 * @param envp Environment
 */
void opt_list_append_opts_from_script_path(const uint32_t prog_number,
                                           ocs::tom::ListPtr<ocs::tom::SPA> &opts_scriptfile, const char *path,
                                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list,
                                           const ocs::tom::ConstList<ocs::tom::SPA> opts_cmdline, char **envp) {
   DENTER(TOP_LAYER);
   char *scriptpath = nullptr;

   if (const auto script_option = opts_cmdline.find_first<SPA_switch_val>(STR_PSEUDO_SCRIPT)) {
      const char *scriptfile = ocs::tom::get<SPA_argval_lStringT>(script_option);

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
   if (const auto c_option = opts_cmdline.find_first<SPA_switch_val>("-C")) {
      prefix = ocs::tom::get<SPA_argval_lStringT>(c_option);
   } else {
      prefix = default_prefix;
   }

   if (answer_list != nullptr) {
      answer_list->reset();
   }

   // parse_script_file() has no typed form yet; it appends to (or creates) the raw list
   lList *scriptfile_lp = opts_scriptfile.release();
   *answer_list = parse_script_file(prog_number, scriptpath, prefix, &scriptfile_lp, envp, FLG_DONT_ADD_SCRIPT);
   opts_scriptfile = ocs::tom::ListPtr<ocs::tom::SPA>::from_typed(scriptfile_lp);

   sge_free(&scriptpath);
   DRETURN_VOID;
}

/**
 * @brief opt_list_append_opts_from_script_path() for an untyped caller
 *
 * @param prog_number the client being parsed for, which decides the option set
 * @param opts_scriptfile embedded command line options (`SPA_Type`), checked; created if it is `nullptr`
 * @param path the root path for the script file
 * @param answer_list AN_Type list
 * @param opts_cmdline Argumente (`SPA_Type`), checked
 * @param envp Environment
 */
void opt_list_append_opts_from_script_path(const uint32_t prog_number, lList **opts_scriptfile, const char *path,
                                           ocs::tom::ListPtr<ocs::tom::AN> *answer_list, const lList *opts_cmdline,
                                           char **envp) {
   ocs::tom::ListPtr<ocs::tom::SPA> scriptfile(*opts_scriptfile);
   opt_list_append_opts_from_script_path(prog_number, scriptfile, path, answer_list,
                                         ocs::tom::ConstList<ocs::tom::SPA>(opts_cmdline), envp);
   *opts_scriptfile = scriptfile.release();
}

/**
 * @brief Merge commandlines together
 *
 * Merge "opts_defaults", "opts_scriptfile" and "opts_cmdline" into
 * "opts_all".
 * Options to a sge submit can come from different sources:
 *  - default settings (sge/sge_request)
 *  - special comments in scriptfiles (override default settings)
 *  - command line options (override default settings and special
 *    comments)
 *
 * @param opts_all destination commandline; created from the first non-empty source if it is an empty handle
 * @param opts_defaults opts from default files, consumed
 * @param opts_scriptfile opts from the script, consumed
 * @param opts_cmdline commandline options, consumed
 */
void opt_list_merge_command_lines(ocs::tom::ListPtr<ocs::tom::SPA> &opts_all,
                                  ocs::tom::ListPtr<ocs::tom::SPA> &&opts_defaults,
                                  ocs::tom::ListPtr<ocs::tom::SPA> &&opts_scriptfile,
                                  ocs::tom::ListPtr<ocs::tom::SPA> &&opts_cmdline) {
   DENTER(TOP_LAYER);
   auto defaults = std::move(opts_defaults);
   auto scriptfile = std::move(opts_scriptfile);
   auto cmdline = std::move(opts_cmdline);

   // Order is very important here
   if (defaults) {
      if (!opts_all) {
         opts_all = std::move(defaults);
      } else {
         opts_all->splice(std::move(defaults));
      }
   }
   if (scriptfile) {
      if (!opts_all) {
         opts_all = std::move(scriptfile);
      } else {
         /* Override the queue (-q) values from defaults; lOverrideStrList() has no typed form yet, it takes
          * the second list over and frees it */
         lOverrideStrList(*opts_all, scriptfile.release(), SPA_switch_val, "-q");
      }
   }
   if (cmdline) {
      if (!opts_all) {
         opts_all = std::move(cmdline);
      } else {
         /* Override queue (-q) values from both defaults and scriptfile */
         lOverrideStrList(*opts_all, cmdline.release(), SPA_switch_val, "-q");
      }
   }

   /* If -ar was requested add -w if it was not explicitly set */
   if (opts_all->find_first<SPA_switch_val>("-ar") && !opts_all->find_first<SPA_switch_val>("-w")) {
      const auto ep_opt = sge_add_arg(opts_all, r_OPT, lIntT, "-w", "e");
      ocs::tom::set<SPA_argval_lIntT>(ep_opt, ERROR_VERIFY);
   }
   DRETURN_VOID;
}

/**
 * @brief Is a certain option contained in list?
 *
 * This function returns true (1) if the given 'option'
 * (e.g. "-help") is contained in the list 'opts'.
 *
 * @param opts SPA_Type list
 * @param option switch name
 *
 * @return found switch? true - yes false - no
 *
 * @see #opt_list_is_X_true
 */
bool opt_list_has_X(const ocs::tom::ConstList<ocs::tom::SPA> opts, const char *option) {
   DENTER(TOP_LAYER);
   const bool ret = static_cast<bool>(opts.find_first<SPA_switch_val>(option));
   DRETURN(ret);
}

/**
 * @brief Check the state of a boolean switch
 *
 * This function returns true (1) if the given 'option'
 * (e.g. "-b") is contained in the list 'opts' and if
 * it was set to 'true'. If the value of the boolean switch
 * is false than the function will also return false (0).
 *
 * @param opts SPA_Type list
 * @param option switch name
 *
 * @return found switch with value 'true' true - yes false - no
 *
 * @see #opt_list_has_X
 */
bool opt_list_is_X_true(const ocs::tom::ConstList<ocs::tom::SPA> opts, const char *option) {
   DENTER(TOP_LAYER);
   bool ret = false;

   if (const auto opt = opts.find_first<SPA_switch_val>(option)) {
      ret = ocs::tom::get<SPA_argval_lIntT>(opt) == 1;
   }
   DRETURN(ret);
}

/** @brief Reject `-masterq` and `-scope` given together
 *
 * `-scope master` is the newer spelling of what `-masterq` used to say, so
 * accepting both would leave two sources for the same setting.
 *
 * @param opts the parsed options (`SPA_Type`)
 * @param alpp receives `STATUS_ESEMANTIC` when both were given
 */
void opt_list_verify_scope(const ocs::tom::ConstList<ocs::tom::SPA> opts, ocs::tom::ListPtr<ocs::tom::AN> *alpp) {
   DENTER(TOP_LAYER);
   if (opts.find_first<SPA_switch_val>("-masterq") && opts.find_first<SPA_switch_val>("-scope")) {
      answer_list_add_sprintf(alpp, STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR, MSG_EITHERSCOPEORMASTERX);
   }
   DRETURN_VOID;
}

/**
 * @brief Creates absolute filename for file in SGE_ROOT
 *
 * Sets the absolut filename of a file in SGE_ROOT in the given dstring
 *
 * @param absolut_filename created absolut filename
 * @param cell_root sge root patch
 * @param filename file name
 *
 * @return pointer to filename in absolut_filename
 *
 * @note MT-NOTE: get_root_file_path() is MT safe
 */
const char *get_root_file_path(dstring *absolut_filename, const char *cell_root, const char *filename) {
   DENTER(TOP_LAYER);
   sge_dstring_sprintf(absolut_filename, "%s/%s", cell_root, filename);
   DRETURN(sge_dstring_get_string(absolut_filename));
}

/**
 * @brief Get absoult filename in users home dir
 *
 * Sets the absolut filename of a file in the users home directory
 *
 * @param home_dir created absolut filename
 * @param user user
 * @param answer_list answer list
 *
 * @return true on success false on error
 *
 * @note MT-NOTE: get_user_home() is MT safe
 */
bool get_user_home(dstring *home_dir, const char *user, ocs::tom::ListPtr<ocs::tom::AN> *answer_list) {
   DENTER(TOP_LAYER);

   if (home_dir == nullptr) {
      DRETURN(false);
   }

   // A test run may point the per user defaults at a directory of its own, so that
   // clusters sharing one operating system user do not share one set of files.
   const char *override_dir = sge_get_user_home_override();
   if (override_dir != nullptr) {
      sge_dstring_copy_string(home_dir, override_dir);
      DRETURN(true);
   }

   passwd pw_struct{};
   const int size = get_pw_buffer_size();
   char *buffer = sge_malloc(size);
   const passwd *pwd = sge_getpwnam_r(user, &pw_struct, buffer, size);
   if (pwd == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_ENOSUCHUSER, ANSWER_QUALITY_ERROR, MSG_USER_INVALIDNAMEX_S, user);
      sge_free(&buffer);
      DRETURN(false);
   }
   if (pwd->pw_dir == nullptr) {
      answer_list_add_sprintf(answer_list, STATUS_EDISK, ANSWER_QUALITY_ERROR, MSG_USER_NOHOMEDIRFORUSERX_S, user);
      sge_free(&buffer);
      DRETURN(false);
   }

   sge_dstring_copy_string(home_dir, pwd->pw_dir);
   sge_free(&buffer);
   DRETURN(true);
}
