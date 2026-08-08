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
 * @brief Reading, setting and rendering queue instance state
 *
 * Every write to `QU_state` goes through #qinstance_set_state, so the state
 * bits and the letters qstat prints cannot drift apart.
 *
 * The state chart the bits below implement:
 *
 * @code
 *
 *         /---------------------------------------------------\
 *         |                     exists                        |
 *         |                                                   |
 * o-----> |                                                   |------->X
 *         |                               /-----------------\ |
 *         |                               |   (suspended)   | |
 *         |                               |                 | |
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !s   |            |    |   s   |    | |
 *         |         |        | <---------------|       |    | | 
 *         |         \--------/            |    \-------/    | |
 *         |- - - - - - - - - - - - - - - -|- - - - - - - - -|-|
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !S   |            |    |   S   |    | |
 *         |         |        | <---------------|       |    | | 
 *         |         \--------/            |    \-------/    | |
 *         |- - - - - - - - - - - - - - - -|- - - - - - - - -|-|
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !A   |            |    |   A   |    | |
 *         |         |        | <---------------|       |    | | 
 *         |         \--------/            |    \-------/    | |
 *         |- - - - - - - - - - - - - - - -|- - - - - - - - -|-|
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !C   |            |    |   C   |    | |
 *         |         |        | <---------------|       |    | | 
 *         |         \--------/            |    \-------/    | |
 *         |                               \-----------------/ |
 *         |- - - - - - - - - - - - - - - - - - - - - - - - - -|
 *         |                               /-----------------\ |
 *         |                               |   (disabled)    | |
 *         |                               |                 | |
 *         |         /--------\            |    /-------\    | |
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !u   |            |    |   u   |    | |
 *         |         |        | <---------------|       |    | |
 *         |         \--------/            |    \-------/    | | 
 *         |- - - - - - - - - - - - - - - -|- - - - - - - - - -|
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !a   |            |    |   a   |    | |
 *         |         |        | <---------------|       |    | |
 *         |         \--------/            |    \-------/    | | 
 *         |- - - - - - - - - - - - - - - -|- - - - - - - - - -|
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !d   |            |    |   d   |    | |
 *         |         |        | <---------------|       |    | | 
 *         |         \--------/            |    \-------/    | |
 *         |- - - - - - - - - - - - - - - -|- - - - - - - - -|-|
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !D   |            |    |   D   |    | |
 *         |         |        | <---------------|       |    | | 
 *         |         \--------/            |    \-------/    | |
 *         |- - - - - - - - - - - - - - - -|- - - - - - - - -|-|
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !E   |            |    |   E   |    | |
 *         |         |        | <---------------|       |    | | 
 *         |         \--------/            |    \-------/    | |
 *         |- - - - - - - - - - - - - - - -|- - - - - - - - -|-|
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !c   |            |    |   c   |    | |
 *         |         |        | <---------------|       |    | | 
 *         |         \--------/            |    \-------/    | |
 *         |- - - - - - - - - - - - - - - -|- - - - - - - - -|-|
 *         |         /--------\            |    /-------\    | |   
 *         | o-----> |        |---------------> |       |    | |
 *         |         |   !o   |            |    |   o   |    | |
 *         |         |        | <---------------|       |    | | 
 *         |         \--------/            |    \-------/    | |
 *         |                               \-----------------/ |
 *         \---------------------------------------------------/
 *
 *         u := qinstance-host is unknown
 *         a := load alarm
 *         s := manual suspended
 *         A := suspended due to suspend_threshold
 *         S := suspended due to subordinate
 *         C := suspended due to calendar
 *         d := manual disabled
 *         D := disabled due to calendar
 *         c := configuration ambiguous
 *         o := orphaned
 *
 * @endcode
 *
 * @see sge_qinstance_state.h
 */

#include <cstring>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "cull/cull_list.h"

#include "sgeobj/sge_conf.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_qinstance.h"
#include "sgeobj/sge_qinstance_state.h"
#include "sgeobj/msg_sgeobjlib.h"

#include "uti/sge.h"

/// Debug layer the queue instance state traces are written to
#define QINSTANCE_STATE_LAYER TOP_LAYER

/* EB: ADOC: add commets */

static const uint32_t states[] = {
      QI_ALARM,
      QI_SUSPEND_ALARM,
      QI_CAL_SUSPENDED,
      QI_CAL_DISABLED,
      QI_DISABLED,
      QI_UNKNOWN,
      QI_ERROR,
      QI_SUSPENDED_ON_SUBORDINATE,
      QI_SUSPENDED,
      QI_AMBIGUOUS,
      QI_ORPHANED, 
      0
   };
   
static const char letters[] = {
      'a',
      'A',
      'C',
      'D',
      'd',
      'u',
      'E',
      'S',
      's',
      'c',
      'o',
      '\0'
   };

/**
 * @brief Set or clear one state bit of a queue instance
 *
 * The single point where `QU_state` is written; the `qinstance_state_set_*`
 * functions all go through it.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @param bit one of the `QI_*` state bits
 * @return true when the bit actually changed
 */
bool
qinstance_set_state(lListElem *this_elem, bool set_state, uint32_t bit)
{
   bool ret = false;
   uint32_t old_state = lGetUlong(this_elem, QU_state);
   uint32_t new_state = old_state;

   if (set_state) {   
      new_state |= bit;
   } else {
      new_state &= ~bit;
   }

   if (old_state != new_state) {
      lSetUlong(this_elem, QU_state, new_state);
      ret = true;
   }

   return ret;
}

/**
 * @brief Checks a qi for a given states
 *
 * Takes a state mask and a queue instance and checks wheather the queue
 * is in at least one of the states. If the state mask contains std::numeric_limits<uint32_t>::max()
 * the function will always return true.
 *
 * @param this_elem queue instance
 * @param bit state mask
 *
 * @return true, if the queue instance has one of the requested states.
 *
 * @note MT-NOTE: qinstance_has_state() is MT safe
 */
bool qinstance_has_state(const lListElem *this_elem, uint32_t bit) {
   bool ret = true;

   if (bit != std::numeric_limits<uint32_t>::max()) {
      ret = (lGetUlong(this_elem, QU_state) & bit) ? true : false;
   }
   return ret;
}

/**
 * @brief Is transition valid
 *
 * Checks if the given transition is valid for a qinstance object.
 * If the transition is valid, than true will be returned by this function.
 *
 * @param transition transition id
 * @param answer_list AN_Type list
 *
 * @return test result true  - transition is valid false - transition is invalid
 *
 * @note MT-NOTE: transition_is_valid_for_qinstance() is MT safe
 */
bool
transition_is_valid_for_qinstance(uint32_t transition, lList **answer_list)
{
   bool ret = false;
  
   transition = transition & (~JOB_DO_ACTION);
   transition = transition & (~QUEUE_DO_ACTION);
   
   if (transition == QI_DO_NOTHING ||
       transition == QI_DO_DISABLE ||
       transition == QI_DO_ENABLE ||
       transition == QI_DO_SUSPEND ||
       transition == QI_DO_UNSUSPEND ||
       transition == QI_DO_CLEARERROR ||
       transition == QI_DO_RESCHEDULE
#ifdef __SGE_QINSTANCE_STATE_DEBUG__
       || transition == QI_DO_SETERROR ||
       transition == QI_DO_SETORPHANED ||
       transition == QI_DO_CLEARORPHANED ||
       transition == QI_DO_SETUNKNOWN ||
       transition == QI_DO_CLEARUNKNOWN ||
       transition == QI_DO_SETAMBIGUOUS ||
       transition == QI_DO_CLEARAMBIGUOUS 
#endif
      ) {
      ret = true;
   } else {
      answer_list_add(answer_list, MSG_QINSTANCE_INVALIDACTION, 
                      STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
   }

   return ret;
}

/* EB: What is the purpose of this function? */
/**
 * @brief Is this a transition option a queue instance accepts?
 *
 * @param option the option to check
 * @param[out] answer_list receives the rejection message
 * @return true when the option is valid
 */
bool
transition_option_is_valid_for_qinstance(uint32_t option, lList **answer_list)
{
   bool ret = false;

   DENTER(QINSTANCE_STATE_LAYER);
   if (option == QI_TRANSITION_NOTHING ||
       option == QI_TRANSITION_OPTION) {
      ret = true;
   } else {
      answer_list_add(answer_list, MSG_QINSTANCE_INVALIDOPTION, 
                     STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
   }
   DRETURN(ret);
}

/**
 * @brief The letter qstat prints for one state bit
 *
 * @param bit one of the `QI_*` state bits
 * @return the letter, or nullptr when the bit is not a displayed state
 */
const char *
qinstance_state_as_string(uint32_t bit)
{
   static const uint32_t states[] = {
      QI_ALARM,
      QI_SUSPEND_ALARM,
      QI_DISABLED,
      QI_SUSPENDED,
      QI_UNKNOWN,
      QI_ERROR,
      QI_SUSPENDED_ON_SUBORDINATE,
      QI_CAL_DISABLED,
      QI_CAL_SUSPENDED,
      QI_AMBIGUOUS,
      QI_ORPHANED,

      (uint32_t)~QI_ALARM,
      (uint32_t)~QI_SUSPEND_ALARM,
      (uint32_t)~QI_DISABLED,
      (uint32_t)~QI_SUSPENDED,
      (uint32_t)~QI_UNKNOWN,
      (uint32_t)~QI_ERROR,
      (uint32_t)~QI_SUSPENDED_ON_SUBORDINATE,
      (uint32_t)~QI_CAL_DISABLED,
      (uint32_t)~QI_CAL_SUSPENDED,
      (uint32_t)~QI_AMBIGUOUS,
      (uint32_t)~QI_ORPHANED,

      /*
       * Don't forget to change the names-array, too
       * if something is changed here
       */

      0 
   };
   static const char *names[23] = { nullptr };
   const char *ret = nullptr;
   int i = 0;

   DENTER(TOP_LAYER);
   if (names[0] == nullptr) {
      names[0] = MSG_QINSTANCE_ALARM;
      names[1] = MSG_QINSTANCE_SUSPALARM;
      names[2] = MSG_QINSTANCE_DISABLED;
      names[3] = MSG_QINSTANCE_SUSPENDED;
      names[4] = MSG_QINSTANCE_UNKNOWN;
      names[5] = MSG_QINSTANCE_ERROR;
      names[6] = MSG_QINSTANCE_SUSPOSUB;
      names[7] = MSG_QINSTANCE_CALDIS;
      names[8] = MSG_QINSTANCE_CALSUSP;
      names[9] = MSG_QINSTANCE_CONFAMB;
      names[10] = MSG_QINSTANCE_ORPHANED;
      names[11] = MSG_QINSTANCE_NALARM;
      names[12] = MSG_QINSTANCE_NSUSPALARM;
      names[13] = MSG_QINSTANCE_NDISABLED;
      names[14] = MSG_QINSTANCE_NSUSPENDED;
      names[15] = MSG_QINSTANCE_NUNKNOWN;
      names[16] = MSG_QINSTANCE_NERROR;
      names[17] = MSG_QINSTANCE_NSUSPOSUB;
      names[18] = MSG_QINSTANCE_NCALDIS;
      names[19] = MSG_QINSTANCE_NCALSUSP;
      names[20] = MSG_QINSTANCE_NCONFAMB;
      names[21] = MSG_QINSTANCE_NORPHANED;
      names[22] = nullptr;
   }

   while (states[i] != 0) {
      if (states[i] == bit) {
         ret = names[i];
         break;
      }
      i++;
   }
   DRETURN(ret);
}

/**
 * @brief Takes a state string and returns an int
 *
 * Takes a string with character representations of the different states and
 * generates a mask with the different states.
 *
 * @param sstate each character one state
 * @param answer_list stores error messages
 * @param filter a bit filter for allowed states
 *
 * @return new state or 0, if no state was set
 *
 * @note MT-NOTE: qinstance_state_from_string() is MT safe
 */
uint32_t
qinstance_state_from_string(const char* sstate, 
                            lList **answer_list, 
                            uint32_t filter){
   uint32_t ustate = 0;
   int i;
   int y;
   bool found = false;
   DENTER(QINSTANCE_STATE_LAYER);

   i=-1;
   while(sstate[++i]!='\0'){
      y=-1;
      found = false;
      while(letters[++y]!='\0'){
         if (letters[y] == sstate[i]) {
            found = true;
            ustate |=  states[y];
            break;
         }
      }

      if ((!found) || ((ustate & ~filter) != 0)){
         ERROR(MSG_QSTATE_UNKNOWNCHAR_CS, sstate[i], sstate);
         answer_list_add(answer_list, SGE_EVENT, STATUS_ENOMGR, ANSWER_QUALITY_ERROR);
         DRETURN(std::numeric_limits<uint32_t>::max());
      }
   }

   if (!found) {
      ustate = std::numeric_limits<uint32_t>::max();
   }

   DRETURN(ustate);
}


/**
 * @brief Render every set state bit as the letters qstat prints
 *
 * @param this_elem the queue instance to read
 * @param[out] string receives the letters, appended
 * @return always true
 *
 * @see #qinstance_state_as_string
 */
bool 
qinstance_state_append_to_dstring(const lListElem *this_elem, dstring *string)
{
   bool ret = true;
   int i = 0;

   DENTER(QINSTANCE_STATE_LAYER);
   while (states[i] != 0) {
      if (qinstance_has_state(this_elem, states[i])) {
         sge_dstring_append_char(string, letters[i]);
      }
      i++;
   }
   sge_dstring_sprintf_append(string, "%c", '\0');

   DRETURN(ret);
}

/**
 * @brief Set or clear #QI_ORPHANED
 *
 * That bit means the queue was deleted but still holds jobs.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_orphaned(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_ORPHANED);
   DRETURN(changed);
}

/**
 * @brief Is #QI_ORPHANED set?
 *
 * That bit means the queue was deleted but still holds jobs.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_orphaned(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_ORPHANED);
}

/**
 * @brief Set or clear #QI_AMBIGUOUS
 *
 * That bit means the cluster queue's configuration does not resolve for this host.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_ambiguous(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_AMBIGUOUS);
   DRETURN(changed);
}

/**
 * @brief Is #QI_AMBIGUOUS set?
 *
 * That bit means the cluster queue's configuration does not resolve for this host.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_ambiguous(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_AMBIGUOUS);
}

/**
 * @brief Set or clear #QI_ALARM
 *
 * That bit means a load threshold is exceeded.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_alarm(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_ALARM);
   DRETURN(changed);
}

/**
 * @brief Is #QI_ALARM set?
 *
 * That bit means a load threshold is exceeded.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_alarm(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_ALARM);
}

/**
 * @brief Set or clear #QI_SUSPEND_ALARM
 *
 * That bit means a suspend threshold is exceeded.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_suspend_alarm(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_SUSPEND_ALARM);
   DRETURN(changed);
}

/**
 * @brief Is #QI_SUSPEND_ALARM set?
 *
 * That bit means a suspend threshold is exceeded.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_suspend_alarm(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_SUSPEND_ALARM);
}

/**
 * @brief Set or clear #QI_DISABLED
 *
 * That bit means an administrator disabled the queue.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_manual_disabled(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_DISABLED);
   DRETURN(changed);
}

/**
 * @brief Is #QI_DISABLED set?
 *
 * That bit means an administrator disabled the queue.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_manual_disabled(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_DISABLED);
}

/**
 * @brief Set or clear #QI_SUSPENDED
 *
 * That bit means an administrator suspended the queue.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_manual_suspended(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_SUSPENDED);
   DRETURN(changed);
}

/**
 * @brief Is #QI_SUSPENDED set?
 *
 * That bit means an administrator suspended the queue.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_manual_suspended(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_SUSPENDED);
}

/**
 * @brief Set or clear #QI_UNKNOWN
 *
 * That bit means the execution host has not reported for too long.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_unknown(lListElem *this_elem, bool set_state)
{
   bool changed;
   DENTER(QINSTANCE_STATE_LAYER);
   if (mconf_get_simulate_execds())
      changed = qinstance_set_state(this_elem, false, QI_UNKNOWN);
   else
      changed = qinstance_set_state(this_elem, set_state, QI_UNKNOWN);
   DRETURN(changed);
}


/**
 * @brief Is #QI_UNKNOWN set?
 *
 * That bit means the execution host has not reported for too long.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_unknown(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_UNKNOWN);
}

/**
 * @brief Set or clear #QI_ERROR
 *
 * That bit means a job could not be started here.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_error(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_ERROR);
   DRETURN(changed);
}

/**
 * @brief Is #QI_ERROR set?
 *
 * That bit means a job could not be started here.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_error(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_ERROR);
}

/**
 * @brief Set or clear #QI_SUSPENDED_ON_SUBORDINATE
 *
 * That bit means a subordinate relation demanded the suspension.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_susp_on_sub(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_SUSPENDED_ON_SUBORDINATE);
   DRETURN(changed);
}

/**
 * @brief Is #QI_SUSPENDED_ON_SUBORDINATE set?
 *
 * That bit means a subordinate relation demanded the suspension.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_susp_on_sub(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_SUSPENDED_ON_SUBORDINATE);
}

/**
 * @brief Set or clear #QI_CAL_DISABLED
 *
 * That bit means the queue's calendar disabled it.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_cal_disabled(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_CAL_DISABLED);
   DRETURN(changed);
}

/**
 * @brief Is #QI_CAL_DISABLED set?
 *
 * That bit means the queue's calendar disabled it.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_cal_disabled(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_CAL_DISABLED);
}

/**
 * @brief Set or clear #QI_CAL_SUSPENDED
 *
 * That bit means the queue's calendar suspended it.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_cal_suspended(lListElem *this_elem, bool set_state)
{
   bool changed;

   DENTER(QINSTANCE_STATE_LAYER);
   changed = qinstance_set_state(this_elem, set_state, QI_CAL_SUSPENDED);
   DRETURN(changed);
}

/**
 * @brief Is #QI_CAL_SUSPENDED set?
 *
 * That bit means the queue's calendar suspended it.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_cal_suspended(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_CAL_SUSPENDED);
}

/**
 * @brief Set or clear #QI_FULL
 *
 * That bit means every slot is in use.
 *
 * @param[in,out] this_elem the queue instance to change
 * @param set_state true to set the bit, false to clear it
 * @return true when the bit actually changed
 */
bool
qinstance_state_set_full(lListElem *this_elem, bool set_state)
{
   return qinstance_set_state(this_elem, set_state, QI_FULL);
}

/**
 * @brief Is #QI_FULL set?
 *
 * That bit means every slot is in use.
 *
 * @param this_elem the queue instance to read
 * @return true when the bit is set
 */
bool 
qinstance_state_is_full(const lListElem *this_elem)
{
   return qinstance_has_state(this_elem, QI_FULL);
}
