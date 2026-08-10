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
 * @brief Path aliases: rewriting paths that differ between hosts
 *
 * The directory structure is not always the same on every submit and execution
 * host, so certain paths have to be mapped per host. Administrators and users
 * activate the mechanism by creating one or both of #PATH_ALIAS_COMMON_FILE
 * and #PATH_ALIAS_HOME_FILE; the file format is documented on
 * `path_alias_read_from_file`.
 *
 * The order is:
 *
 * 1. a submit client determines the current working directory, then reads the
 *    cluster global alias file and the user's own one after it, as if the
 *    second were appended to the first;
 * 2. the resulting alias information travels with the job;
 * 3. on the execution host it is evaluated, and the leading part of the
 *    working directory is replaced where an entry's host matches the executing
 *    host.
 *
 * @see sge_path_alias.h
 */

#include <cstring>
#include <pwd.h>
#include <cerrno>

#include "uti/sge_hostname.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_string.h"
#include "uti/sge_uidgid.h"
#include "uti/sge_unistd.h"
#include "uti/sge_stdlib.h"

#include "comm/lists/cl_errors.h"

#include "sgeobj/sge_host.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_job.h"
#include "sgeobj/sge_utility.h"
#include "sgeobj/sge_path_alias.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "msg_common.h"
#include "msg_daemons_common.h"

static int path_alias_read_from_file(lList **path_alias_list, lList **alpp, 
                                     char *file_name);

/**
 * @brief Read file content to list
 *
 * Read and parse the file with the name "file_name" and append
 * entries into "path_alias_list". Errors will be logged in "alpp".
 * File format:
 * - Blank lines and lines beginning with a # sign in the first
 *   column are skipped.
 * - Each line - other than a blank line or a line preceded by # -
 *   must contain four strings separated by any number of blanks
 *   or tabs.
 * - The first string specifies a source path, the second a submit
 *   host, the third an execution host, and the fourth the source
 *   path replacement.
 * - Both the submit and the execution host entries may consist
 *   of only a * sign, which matches any host.
 *
 * @param path_alias_list PA_Type list pointer
 * @param alpp AN_Type list pointer
 * @param file_name name of an alias file
 *
 * @return error state -1 - Error 0 - OK
 *
 * @note MT-NOTE: path_alias_read_from_file() is MT safe
 */
static int path_alias_read_from_file(lList **path_alias_list, lList **alpp,
                                     char *file_name) {
   DENTER(GDI_LAYER);

   FILE *fd;
   char buf[10000];
   char err[MAX_STRING_SIZE];
   char origin[SGE_PATH_MAX];
   char submit_host[SGE_PATH_MAX];
   char exec_host[SGE_PATH_MAX];
   char translation[SGE_PATH_MAX];
   lListElem *pal;
   SGE_STRUCT_STAT sb;
   int ret = 0;

   if (!path_alias_list || !file_name) {
      CRITICAL(MSG_SGETEXT_NULLPTRPASSED_S, __func__);
      answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      DRETURN(-1);
   }

   if ((SGE_STAT(file_name, &sb) != 0) && (errno == ENOENT)) {
      DRETURN(0);
   }    

   if (!(fd=(fopen(file_name, "r")))) {
      DRETURN(-1);
   }

   while (fgets(buf, sizeof(buf), fd)) {
      char *crp;

      /* strip \n */
      if ((crp = strchr(buf, (int)'\n')))
         *crp = '\0';

      DPRINTF("Path Alias: >%s<\n",buf);

      /* skip empty lines and comments */
      if (!strlen(buf) || (*buf == '#' )) {
         continue;
      }

      /*
       * reset
       */
      origin[0]      = '\0';
      submit_host[0] = '\0';
      exec_host[0]   = '\0';
      translation[0] = '\0';   

      sscanf(buf, "%s %s %s %s", origin, submit_host, exec_host, translation);

      /*
       * check for correctness of path alias file
       */
      if (*origin == '\0' || *submit_host == '\0' || *exec_host == '\0' || *translation == '\0') {
         snprintf(err, sizeof(err), MSG_ALIAS_INVALIDSYNTAXOFPATHALIASFILEX_S, file_name);
         answer_list_add(alpp, err, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         ret = -1;
         break;
      }

      /*
       * compress multiple slashes to one slash
       */
      sge_compress_slashes(origin);
      sge_compress_slashes(translation);
            
      
      pal = lAddElemStr(path_alias_list, PA_origin, origin, PA_Type);
      
      if (!pal) {
         answer_list_add(alpp, MSG_SGETEXT_NOMEM, STATUS_EMALLOC, ANSWER_QUALITY_ERROR);
         ret = -1;
         break;
      }

      /*
       * set the values of the element
       */
      lSetHost(pal, PA_submit_host, submit_host);
      if ( strcmp(submit_host, "*") && (sge_resolve_host(pal, PA_submit_host) != CL_RETVAL_OK)) {
         snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_SGETEXT_CANTRESOLVEHOST_S, submit_host);
         answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
         ret = -1;
         break;
      }
      lSetHost(pal, PA_exec_host, exec_host);
      lSetString(pal, PA_translation, translation);

   } /* while (fgets) */

   FCLOSE(fd);

   DRETURN(ret);
FCLOSE_ERROR:
   snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_FILE_ERRORCLOSEINGXY_SS, file_name, strerror(errno));
   answer_list_add(alpp, SGE_EVENT, STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
   return -1;
}

/**
 * @brief Initialize path_alias_list
 *
 * Intitialize "path_alias_list" according to the different
 * path aliasing files.
 * Following files will be used if available:
 *    $SGE_ROOT/$CELL/common/sge_aliases
 *    $HOME/.sge_aliases
 *
 * @param path_alias_list PA_Type list pointer
 * @param alpp AN_Type list pointer
 * @param user username
 * @param host hostname
 * @param cell_root the cell directory the common alias file lives under
 *
 * @return return state -1 - error 0 - OK
 *
 * @note MT-NOTE: path_alias_list_initialize() is MT safe
 */
int path_alias_list_initialize(lList **path_alias_list,
                               lList **alpp,
                               const char *cell_root,
                               const char *user,
                               const char *host) {
   DENTER(TOP_LAYER);

   char filename[2][SGE_PATH_MAX];
   char err[MAX_STRING_SIZE];
   /* 
    * find names of different sge_path_alias files:
    *    global
    *    home directory
    */
   {
      struct passwd *pwd;
      struct passwd pw_struct{};
      char *buffer;
      int size;

      size = get_pw_buffer_size();
      buffer = sge_malloc(size);
      pwd = sge_getpwnam_r(user, &pw_struct, buffer, size);

      if (!pwd) {
         snprintf(err, sizeof(err), MSG_USER_INVALIDNAMEX_S, user);
         answer_list_add(alpp, err, STATUS_ENOSUCHUSER, ANSWER_QUALITY_ERROR);
         sge_free(&buffer);
         DRETURN(-1);
      }
      if (!pwd->pw_dir) {
         snprintf(err, sizeof(err), MSG_USER_NOHOMEDIRFORUSERX_S, user);
         answer_list_add(alpp, err, STATUS_EDISK, ANSWER_QUALITY_ERROR);
         sge_free(&buffer);
         DRETURN(-1);
      }
      snprintf(filename[0], sizeof(filename[0]), "%s/%s", cell_root, PATH_ALIAS_COMMON_FILE);
      snprintf(filename[1], sizeof(filename[1]), "%s/%s", pwd->pw_dir, PATH_ALIAS_HOME_FILE);

      sge_free(&buffer);
   }

   /*
    * read files
    */
   {
      int i;

      for (i=0; i<2; i++) {
         if (path_alias_read_from_file(path_alias_list, alpp, filename[i]) != 0) {
            snprintf(err, sizeof(err), MSG_ALIAS_CANTREAD_SS, filename[i], strerror(errno));
            answer_list_add(alpp, err, STATUS_EDISK, ANSWER_QUALITY_ERROR);
            DRETURN(-1);
         }
      }
   }

   /*
    * remove the unnecessary hosts from the list
    */
   {
      lCondition *where = nullptr;

      where = lWhere("%T(%I == %s || %I == %s)", PA_Type, 
                     PA_submit_host, "*", PA_submit_host, host);
      *path_alias_list = lSelectDestroy(*path_alias_list, where);
      lFreeWhere(&where);
   }

   DRETURN(0);
}

/**
 * @brief Map path according alias table
 *
 * "path_aliases" is used to map "inpath" for the host "myhost"
 * into its alias path which will be written into the buffer
 * "outpath" of size "outmax".
 *
 * @param path_aliases alias table (PA_Type)
 * @param alpp AN_Type list pointer
 * @param inpath input path
 * @param myhost hostname
 * @param outpath result path
 *
 * @return return state 0 - OK
 *
 * @note MT-NOTE: path_alias_list_get_path() is MT safe
 */
int path_alias_list_get_path(const lList *path_aliases, lList **alpp,
                             const char *inpath, const char *myhost,
                             dstring *outpath) {
   DENTER(TOP_LAYER);
   const char *origin;
   const char *translation;
   const char *exec_host;
   dstring the_path = DSTRING_INIT;

   sge_dstring_copy_string(outpath, inpath);
   sge_dstring_copy_dstring(&the_path, outpath); 

   if (path_aliases && lGetNumberOfElem(path_aliases) > 0) { 
      for_each_rw_lv (pap, path_aliases) {
         size_t orign_str_len = 0; 
         origin = lGetString(pap, PA_origin);
         orign_str_len = strlen(origin);
         exec_host = lGetHost(pap, PA_exec_host);
         translation = lGetString(pap, PA_translation);

         if (strncmp(origin, sge_dstring_get_string(&the_path), 
             orign_str_len )) {
            /* path leaders aren't the same ==> no match */
            continue;
         }
 
         /* the paths are ok, what about the exec hosts ? */
         /* if exec_host is a '*' we have a match */
         if (*exec_host != '*') {
            /* no '*', so we have to look closer   */
            /* resolv the exec host from the alias */
            if (sge_resolve_host(pap, PA_exec_host) != CL_RETVAL_OK) {
               ERROR(MSG_SGETEXT_CANTRESOLVEHOST_S, exec_host);
               continue;
            }
            exec_host = lGetHost(pap, PA_exec_host);

            /* and compare it to the executing host */
            if (sge_hostcmp(exec_host, myhost))
               continue;

         }
 
         /* copy the alias as leading part of cwd */
         sge_dstring_copy_string(outpath, translation);
 
         /* now append the trailer of the original cwd */
         {  
            const char *path = sge_dstring_get_string(&the_path);
            sge_dstring_append(outpath, path + orign_str_len );
         }

         DPRINTF("Path " SFQ " has been aliased to " SFQ "\n", inpath, sge_dstring_get_string(outpath));
 
         /* and we have to start all over again for subsequent aliases */
         sge_dstring_copy_dstring(&the_path, outpath);
      }
   } else {
      DPRINTF("\"path_aliases\" containes no elements\n");
   }

   sge_dstring_free(&the_path);

   DRETURN(0);
}

/**
 * @brief Verify a path string
 *
 * Verifies if a path string has valid contents.
 *
 * @param path the string to verify
 * @param answer_list answer list to pass back error messages
 * @param name name of the path to check, e.g. "prolog" for error output
 * @param absolute does it have to be an absolute path?
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: path_verify() is MT safe
 */
bool path_verify(const char *path, lList **answer_list, const char *name, bool absolute) {
   bool ret = true;

   if (path == nullptr || *path == '\0') {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                              MSG_PATH_ALIAS_INVALID_PATH);
      ret = false;
   }

   if (ret) {
      if (strlen(path) > SGE_PATH_MAX) {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_PATH_TOOLONG_I, SGE_PATH_MAX);
         ret = false;
      }
   }

   /* check for absolute path */
   if (absolute) {
      if (path[0] != '/') {
         answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR, 
                                 MSG_CONF_THEPATHGIVENFORXMUSTSTARTWITHANY_S, name);
         ret = false;
      }
   }

   /* 
    * TODO: we have to do additional checks here 
    * we could use a variant of verify_str_key, using different valid character definition
    * verify_str_key will be extended to do length check
    * shall we also check for absolute vs. relative path?
    * filename vs. directory path?
    */

   return ret;
}

/**
 * @brief Verify path alias list
 *
 * Verify a path alias list as it is sent with job or pe task start orders
 * to sge_execd.
 *
 * @param path_aliases path alias list
 * @param answer_list answer list to pass back error messages
 *
 * @return true on success, false on error with error message in answer_list
 *
 * @note MT-NOTE: path_alias_verify() is MT safe
 *
 * @see #path_verify
 */
bool path_alias_verify(const lList *path_aliases, lList **answer_list) {
   bool ret = true;

   for_each_ep_lv(ep, path_aliases) {
      /* 
       * PA_origin and PA_translation may not be nullptr or empty string
       * they have to be valid paths.
       */
      if (ret) {
         ret = path_verify(lGetString(ep, PA_origin), answer_list, "path_alias: origin", false);
      }
      if (ret) {
         ret = path_verify(lGetString(ep, PA_translation), answer_list, "path_alias: translation", false);
      }

       /*
       * PA_submit_host and PA_exec_host have to be either '*' or a valid
       * hostname (no need to resolve them).
       */
      if (ret) {
         ret = verify_host_name(answer_list, lGetHost(ep, PA_submit_host));
      }
      if (ret) {
         ret = verify_host_name(answer_list, lGetHost(ep, PA_exec_host));
      }

      if (!ret) {
         break;
      }
   }

   return ret;
}

/**
 * @brief Verify a path list
 *
 * Verify a path list, e.g. the path specification in JB_stdout_path_list,
 * coming from a qsub -o `path_list`.
 *
 * @param path_list the path list to verify
 * @param answer_list answer list to pass back error messages
 * @param name name of the checked attribute for error output
 *
 * @return true: everything ok, else false
 *
 * @note MT-NOTE: path_list_verify() is MT safe
 */
bool path_list_verify(const lList *path_list, lList **answer_list, const char *name) {
   bool ret = true;

   for_each_ep_lv(ep, path_list) {
      const char *host;

      ret = path_verify(lGetString(ep, PN_path), answer_list, name, false);
      if (!ret) {
         break;
      }

      host = lGetHost(ep, PN_host);
      if (host != nullptr) {
         ret = verify_host_name(answer_list, host);
         if (!ret) {
            break;
         }
      }

      host = lGetHost(ep, PN_file_host);
      if (host != nullptr) {
         ret = verify_host_name(answer_list, host);
         if (!ret) {
            break;
         }
      }
   }

   return ret;
}
