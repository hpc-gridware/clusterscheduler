#pragma once
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
 * @brief The process data collector: what each job on this host is consuming
 *
 * The execution daemon needs per-job CPU, memory and I/O usage, but the
 * operating system accounts per *process*. The collector bridges that: it
 * walks `/proc` on every load interval, attributes each process to a job by
 * its additional group id, and keeps a running total per job.
 *
 * Totals have to be accumulated rather than read, because a process that has
 * already exited is gone from `/proc` - so its usage is added to the job's
 * total before it disappears, and the job's numbers only ever grow.
 *
 * The jobs and their processes are held in the doubly linked circular lists
 * described below.
 */

#include "sgeobj/sge_conf.h"

#include "err_trace.h"
#include "exec_ifm.h"

/* The offsetof macro is part of ANSI C, but many compilers lack it, for
 * example "gcc -ansi"
  */
#if !defined (offsetof)
/** @brief Byte offset of a member within its struct
 * @param type the struct type
 * @param member the member
 */
#  define offsetof(type, member) \
         ((size_t)((char *)&((type *)0L)->member - (char *)0L))
#endif

/*

   The following are some macros for managing doubly linked circular lists.
   These linked lists are linked together using the lnk_link_s structure. The
   next and prev pointers in the lnk_link_s structure always point to another
   lnk_link_s structure. This allows objects to easily reside on multiple
   lists by having multiple lnk_link_s structures in the object. To define
   an entry that will be linked in a list, you simply add the lnk_link_s to
   the object;

       typedef struct str_elem_s {
	  lnk_link_t link;
	  char *str;
       } str_elem_t;

   
   The head of a list is also a lnk_link_s structure. The next entry of the
   head link points to the first entry in the list. The prev entry of the head
   link points to the last entry in the list. The list is always maintained as
   a circular list which contains the head link. That is, the prev pointer of
   the first entry in the list points to the head link and the next pointer
   of the last entry in the list points to the head link. The LNK_INIT macro
   is used to initialize a list. Specifically, it sets up the head link.
   To define a list:

	lnk_link_t list;
	LNK_INIT(&list);

   The LNK_ADD routine adds an entry to a list. The entry identified by link2
   is inserted into a list before the entry identified as link1.  To add to
   the beginning of a list:

       str_elem_t *str = strdup("Hello world\n");

       LNK_ADD(list->next, &str->link);

   To add to the end of a list:

       LNK_ADD(list->prev, &str->link);

   The LNK_DATA macro returns the data associated with an element in the
   list. The arguments are the entry link, the structure name of the 
   entry object, and the link field. The LNK_DATA macros calculates where
   the data is based on where the link field is in the structure object.

       str_elem_t *elem;
       lnk_link_t *curr;

       for (curr=list.next; curr != &list; curr=curr->next) {
	  elem = LNK_DATA(curr, str_elem_t, link);
          puts(elem->str);
       }


   The LNK_DELETE macro removes an element from a list.

       while((curr=list.next) != &list) {
	   elem = LNK_DATA(curr, str_elem_t, link);
	   free(elem->str);
	   LNK_DELETE(curr);
       }

*/

/** @brief One link in a doubly linked circular list
 *
 * An object joins a list by embedding one of these, so an object that has to
 * be on several lists at once simply embeds several. #LNK_DATA converts a link
 * back to the object that contains it.
 *
 * The list head is a link too: its `next` points at the first entry and its
 * `prev` at the last, and the list always contains the head, so there is no
 * end-of-list special case.
 */
typedef struct lnk_link_s {
    struct lnk_link_s   *next;   ///< The following entry, or the head after the last one
    struct lnk_link_s   *prev;   ///< The preceding entry, or the head before the first one
} lnk_link_t;

/** @name Doubly linked circular list operations
 * @{
 */

/** @brief The object that contains a link
 *
 * Works back from the address of the embedded link to the start of the object,
 * which is why an object can sit on several lists at once.
 *
 * @param link the link
 * @param entry_type the type of the containing object
 * @param link_field the name of the embedded link within it
 */
#define LNK_DATA(link, entry_type, link_field)  \
    ((entry_type *)((char *)(link) - offsetof(entry_type, link_field)))

#if 0
#define LNK_INIT(link)                          \
(                                               \
    (link)->next = (link),                      \
    (link)->prev = (link),                      \
    (link)                                      \
)
#endif
/** @brief Make a link the head of an empty list, pointing at itself
 * @param link the head
 */
#define LNK_INIT(link)                          \
(                                               \
    (link)->next = (link),                      \
    (link)->prev = (link)                      \
)

/** @brief Insert an entry after a given link
 *
 * Passing `head->next` prepends and `head->prev` appends, because the head is
 * part of the ring.
 *
 * @param link1 the link to insert after
 * @param link2 the entry being inserted
 */
#define LNK_ADD(link1, link2)                   \
{                                               \
    lnk_link_t  *zzzlink = (link1);             \
    (link2)->next = (zzzlink)->next;            \
    (link2)->prev = (zzzlink);                  \
    (zzzlink)->next->prev = (link2);            \
    (zzzlink)->next = (link2);                  \
}

/** @brief Unlink an entry from its list
 *
 * The entry's own pointers are left as they are; the caller is expected to
 * free it or re-link it straight away.
 *
 * @param link the entry to remove
 */
#define LNK_DELETE(link)                        \
{                                               \
    lnk_link_t  *zzzlink = (link);              \
    (zzzlink)->prev->next = (zzzlink)->next;    \
    (zzzlink)->next->prev = (zzzlink)->prev;    \
}

/** @} */

typedef struct psJob_s psJob_t;    ///< The usage figures reported for one job
typedef struct psProc_s psProc_t;  ///< The usage figures read for one process
typedef struct psStat_s psStat_t;  ///< Statistics about the collector itself

/** @brief One watched job and everything the collector knows about it
 *
 * The totals at the end are the collector's own running sums, kept separately
 * from #job because a process that exits between two intervals must still
 * contribute what it used.
 */
typedef struct {
   lnk_link_t link;        ///< Links this job into the collector's job list
   psJob_t job;            ///< The figures handed out to the caller
   usage_collection_t usage_collection; ///< USAGE_COLLECTION_DEFAULT, ...
   lnk_link_t procs;       ///< Head of this job's process list
   lnk_link_t arses;       ///< Head of this job's accounting records, where the platform has them
   time_t precreated;     ///< set if job element created before psWatchJob
   time_t starttime;       ///< When the job was first seen
   time_t timeout;        ///< completion timeout
   double utime;          ///< user time
   double stime;          ///< system time
   uint64 mem;             ///< Integral memory usage, accumulated
   uint64 chars;           ///< Characters transferred, accumulated
} job_elem_t;

/** @brief One process belonging to a watched job
 *
 * The `delta_*` fields are what this interval contributed; they are added to
 * the job's totals and then reset. The plain counters below them hold what the
 * previous interval read, so that the delta can be computed from two absolute
 * readings.
 */
typedef struct {
   lnk_link_t link;         ///< Links this process into its job's process list
   JobID_t jid;             ///< The job this process was attributed to
   psProc_t proc;           ///< The figures read from /proc
   uint64 chars;            ///< Characters transferred, accumulated for this process
   uint64 mem;              ///< delta integral vmem
   uint64 vmem;             ///< virtual process size
   uint64 rss;              ///< resident set size
   uint64 ru_ioblock;       ///< # of block input operations
   uint64 delta_chars;      ///< number of chars to be added to jd_chars this time step
   uint64 delta_ioops;      ///< number of io operations to be added to jd_ioops this time step
   double delta_iow;        ///< I/O wait time in seconds (with fractional part) to be added in this time step
#if defined(LINUX)
   uint64 iochars;          ///< number of chars from the previous load interval
   uint64 ioops;            ///< number of operations from the previous load interval
   double iow;              ///< I/O wait time in seconds (with fractional part)
   uint64 pss;              ///< proportional set size
   uint64 pmem;             ///< private memory
   uint64 smem;             ///< shared memory
#endif
} proc_elem_t;

extern long pagesize;   ///< The system page size, read once and reused for every /proc conversion

#if defined(LINUX)
   int sup_groups_in_proc();
#endif


#if defined(LINUX) || defined(SOLARIS) || defined(FREEBSD) || defined(DARWIN)
   void pdc_kill_addgrpid(gid_t, int, tShepherd_trace);
#endif

int		psStartCollector();
int		psStopCollector();
int		psWatchJob(JobID_t JobID, usage_collection_t usage_collection);
int		psIgnoreJob(JobID_t JobID);

/** @brief Statistics about the collector itself
 *
 * @return the statistics
 *
 * @warning Declared here but defined nowhere in the tree, and called from
 *          nowhere either. Kept because removing a declaration is a code
 *          change; see the dead-declaration list.
 */
struct psStat_s	*psStatus();

struct psJob_s *psGetOneJob(JobID_t JobID);
struct psJob_s *psGetAllJobs();
