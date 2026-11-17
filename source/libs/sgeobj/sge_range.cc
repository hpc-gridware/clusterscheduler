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
 * @brief Range lists: compact sets of numbers such as array task ids
 *
 * A range is a `min`-`max`-`step` triple (`RN_Type`); a range list holds
 * several of them and is kept sorted in descending order with no two ranges
 * overlapping. That representation is what lets an array job with a million
 * tasks be stored, spooled and printed as `1-1000000:1` instead of a million
 * elements.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <utility>

#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"

#include "sgeobj/sge_range.h"
#include "sgeobj/sge_answer.h"
#include "sgeobj/msg_sgeobjlib.h"
#include "sgeobj/tom/ocs_tom_RN.h"
#include "tom/ocs_Element.h"
#include "tom/ocs_List.h"

/// Characters separating two ranges in a textual range list
#define RANGE_SEPARATOR_CHARS ","
/// Debug layer the range list traces are written to
#define RANGE_LAYER BASIS_LAYER


static bool range_is_overlapping(ocs::tom::ConstElement<ocs::tom::RN> range1,
                                 ocs::tom::ConstElement<ocs::tom::RN> range2);
static void ocs_range_list_insert_id(ocs::tom::List<ocs::tom::RN> list, uint32_t id);

/**
 * @brief Merge a range into a range list that is kept in descending order without overlaps
 *
 * The list is created if it does not exist yet. The range is taken over: it
 * is either linked into the list or, when the list covers it already or it
 * was merged into a range of the list, freed.
 *
 * @param r the range (`RN_Type`), which the function takes over
 * @param rl in-out: the `RN_Type` list, created if the handle is empty
 *
 * @note MT-NOTE: expand_range_list() is MT safe
 */
static void expand_range_list(ocs::tom::ElementPtr<ocs::tom::RN> &&r, ocs::tom::ListPtr<ocs::tom::RN> &rl) {
   DENTER(TOP_LAYER);

   const auto rmin = ocs::tom::get<RN_min>(*r);
   const auto rmax = ocs::tom::get<RN_max>(*r);
   const auto rstep = ocs::tom::get<RN_step>(*r);

   /* create list */
   if (!rl) {
      rl = ocs::tom::ListPtr<ocs::tom::RN>::create("ranges");
   }
   const auto list = *rl;

   if (!list.empty()) {
      auto ep = list.front();
      while (ep) {

         if (rstep != ocs::tom::get<RN_step>(ep) || rstep > 1 || ocs::tom::get<RN_step>(ep) > 1) {
            list.push_front(std::move(r));
            break;
         } else if (rmin > ocs::tom::get<RN_max>(ep)) {

            /*
             ** r and ep are non-overlapping and r is to be
             ** sorted ahead of ep.
             */
            list.push_front(std::move(r));
            break;

         } else if (rmax < ocs::tom::get<RN_min>(ep)) {
            /*
             ** r and ep are non-overlapping and r is to be
             ** sorted after ep ==> go for the next iteration
             */
            ep = ep.next();
            if (!ep) {
               /* We are at the end of the list ==> append r */
               list.push_back(std::move(r));
            }

            continue;

         } else if ((rmax <= ocs::tom::get<RN_max>(ep)) &&
                    (rmin >= ocs::tom::get<RN_min>(ep))) {

            /*
             ** r is fully contained within ep ==> delete it
             */
            r.reset();
            break;

         } else if (rmin < ocs::tom::get<RN_min>(ep)) {

            /*
             ** ep is either fully contained within r
             ** or r overlaps ep at ep's bottom ==>
             ** overwrite ep and see if further list elements
             ** are covered also;
             */
            if (rmax > ocs::tom::get<RN_max>(ep)) {
               /* r contains ep */
               ocs::tom::set<RN_max>(ep, rmax);
            }
            ocs::tom::set<RN_min>(ep, rmin);

            /* we're already sure we can free r */
            r.reset();
            const auto rr = ep;
            ep = ep.next();
            while (ep) {
               if (rmin < ocs::tom::get<RN_min>(ep)) {

                  /* it also contains this one */
                  const auto covered = ep;
                  ep = ep.next();
                  list.erase(covered);

               } else if (rmin <= ocs::tom::get<RN_max>(ep)) {

                  /* these overlap ==> glue them */
                  ocs::tom::set<RN_min>(rr, ocs::tom::get<RN_min>(ep));

                  const auto glued = ep;
                  ep = ep.next();
                  list.erase(glued);
                  break;

               } else {
                  /* they are non overlapping */
                  break;
               }

               ep = ep.next();
            }

            /* we're now sure we inserted it */
            break;
         } else if (rmax > ocs::tom::get<RN_max>(ep)) {
            /*
             ** r and ep overlap and r starts above ep
             ** ==> glue them
             */
            ocs::tom::set<RN_max>(ep, rmax);
            r.reset();
            break;
         }
      }
   } else {
      list.push_back(std::move(r));
   }

   DRETURN_VOID;
}

/**
 * @brief Correct end of a range element
 *
 * This function modifies the the 'end' id of the 'this_range'
 * element if it is not correct. After the modification the
 * 'end' id is the last valid id which is part of the range.
 *
 * @code
 * 1-6:2 (1,3,5) will be modified to 1-5:2
 * @endcode
 *
 * @param this_range the range (`RN_Type`); an empty handle is a no-op
 *
 * @note 'this_range' will be modified
 */
void range_correct_end(const ocs::tom::Element<ocs::tom::RN> this_range) {
   DENTER(RANGE_LAYER);
   if (this_range) {
      uint32_t start, end, step;

      range_get_all_ids(this_range, &start, &end, &step);
      if (step > 0) {
         if ((end - start) % step) {
            const uint32_t factor = (end - start) / step;

            end = start + factor * step;
            range_set_all_ids(this_range, start, end, step);
         }
      } else {
         step = end - start;
      }
      range_set_all_ids(this_range, start, end, step);
   }
   DRETURN_VOID;
}

/**
 * @brief Do two ranges interleave?
 *
 * True will be returned when the given ranges interleave. This
 * does not necessaryly mean that certain ids exist in both ranges.
 *
 * @code
 * 1-5:3    4-10:7      => true
 * 1-5:3    5-10:6      => true
 * 1-5:3    6-10:4      => false
 * @endcode
 *
 * @param this_elem the first range (`RN_Type`)
 * @param range the second range (`RN_Type`)
 *
 * @return false or true; false if one of the handles is empty
 *
 * @note MT-NOTE: range_is_overlapping() is MT safe
 */
static bool range_is_overlapping(const ocs::tom::ConstElement<ocs::tom::RN> this_elem,
                                 const ocs::tom::ConstElement<ocs::tom::RN> range) {
   DENTER(RANGE_LAYER);

   bool ret = false;

   if (this_elem && range) {
      uint32_t start1, end1, step1;
      uint32_t start2, end2, step2;

      range_get_all_ids(this_elem, &start1, &end1, &step1);
      range_get_all_ids(range, &start2, &end2, &step2);
      if (end1 >= start2) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/**
 * @brief (Re)initialize a range list
 *
 * 'this_list' will be created if it does not exist. If it already
 * exists all elements contained in this list will be removed.
 *
 * @param this_list in-out: the RN_Type list; afterwards an empty list
 * @param answer_list Pointer to a AN_Type-list or nullptr; unused since creating the list throws on a lack
 *                    of memory instead of failing
 */
void range_list_initialize(ocs::tom::ListPtr<ocs::tom::RN> &this_list, [[maybe_unused]] lList **answer_list) {
   DENTER(RANGE_LAYER);
   if (this_list) {
      const auto list = *this_list;
      auto next_range = list.front();

      while (next_range) {
         const auto range = next_range;

         next_range = range.next();
         list.erase(range);
      }
   } else {
      this_list = ocs::tom::ListPtr<ocs::tom::RN>::create("");
   }
   DRETURN_VOID;
}

/**
 * @brief range_list_initialize() for an untyped caller
 *
 * @param this_list Pointer to a RN_Type-list, checked; nothing happens for nullptr
 * @param answer_list Pointer to a AN_Type-list or nullptr
 */
void range_list_initialize(lList **this_list, lList **answer_list) {
   if (this_list != nullptr) {
      ocs::tom::ListPtr<ocs::tom::RN> list(*this_list);
      range_list_initialize(list, answer_list);
      *this_list = list.release();
   }
}

/**
 * @brief Determines the number of ids
 *
 * This function determines the number of ids contained
 * in 'this_list'. If 'this_list' is nullptr then 0 will be returned.
 *
 * @code
 * 1-5:2, 7-10:3, 20-23:1 (1, 3, 5, 7, 10, 20, 21, 22, 23) => 9
 * @endcode
 *
 * @param this_list RN_Type list, may be an empty handle
 *
 * @return number of ids
 *
 * @note MT-NOTE: range_list_get_number_of_ids() is MT safe
 */
uint32_t range_list_get_number_of_ids(const ocs::tom::ConstList<ocs::tom::RN> this_list) {
   DENTER(RANGE_LAYER);
   uint32_t ret = 0;

   for (const auto range : this_list) {
      ret += range_get_number_of_ids(range);
   }
   DRETURN(ret);
}

/**
 * @brief range_list_get_number_of_ids() for an untyped caller
 *
 * @param this_list RN_Type list, checked; may be nullptr
 *
 * @return as range_list_get_number_of_ids()
 */
uint32_t range_list_get_number_of_ids(const lList *this_list) {
   return range_list_get_number_of_ids(ocs::tom::ConstList<ocs::tom::RN>(this_list));
}

/**
 * @brief Count the distinct id values in a single RN_Type range element.
 *
 * Computes @c 1 + (max - min) / step using unsigned integer division.
 * Integer division truncates towards zero, so a stored @c max that falls off
 * the step grid is silently ignored and the result is still correct.
 * Example: [1, 6, 2] → @c 1 + (6-1)/2 = 3  (ids: 1, 3, 5).
 *
 * @pre @p this_elem must have a non-zero step (i.e. it must have been
 *      initialised via range_set_all_ids()).  Calling this on a freshly
 *      created zeroed element (step == 0) causes division by zero.
 *
 * @param this_elem  The RN_Type element to query.
 * @return           Number of distinct ids in the range.
 *
 * @see range_correct_end
 * @see range_list_get_number_of_ids
 */
uint32_t range_get_number_of_ids(const ocs::tom::ConstElement<ocs::tom::RN> this_elem) {
   DENTER(RANGE_LAYER);

   uint32_t start, end, step;

   range_get_all_ids(this_elem, &start, &end, &step);
   const uint32_t ret = 1 + (end - start) / step;
   DRETURN(ret);
}

/**
 * @brief Print range list into the string
 *
 * Print all ranges given in 'this_list' into the dynamic 'string'.
 * If 'this_list' is nullptr then the word "UNDEFINED" will be added
 * to 'string'. If 'ignore_step' is 'true' then the stepsize of all
 * ranges will be suppressed.
 *
 * @param this_list RN_Type list, may be an empty handle
 * @param string dynamic string; nothing happens for nullptr
 * @param ignore_step ignore step for printing
 * @param comma_as_separator use the format 1,2,3 instead of 1-2:
 * @param print_always_as_range even if the range has only one id
 *
 * @note string will be modified
 */
void range_list_print_to_string(const ocs::tom::ConstList<ocs::tom::RN> this_list, dstring *string,
                                const bool ignore_step, const bool comma_as_separator,
                                const bool print_always_as_range) {
   DENTER(RANGE_LAYER);
   if (string != nullptr) {
      if (this_list) {
         for (const auto range : this_list) {
            uint32_t start, end, step;

            range_get_all_ids(range, &start, &end, &step);
            range_to_dstring(start, end, step, string, ignore_step,
                             comma_as_separator, print_always_as_range);
         }
      } else {
         sge_dstring_append(string, "UNDEFINED");
      }
   }
   DRETURN_VOID;
}

/**
 * @brief range_list_print_to_string() for an untyped caller
 *
 * @param this_list RN_Type list, checked; may be nullptr
 * @param string dynamic string
 * @param ignore_step ignore step for printing
 * @param comma_as_separator use the format 1,2,3 instead of 1-2:
 * @param print_always_as_range even if the range has only one id
 */
void range_list_print_to_string(const lList *this_list, dstring *string, bool ignore_step,
                                bool comma_as_separator, bool print_always_as_range) {
   range_list_print_to_string(ocs::tom::ConstList<ocs::tom::RN>(this_list), string, ignore_step,
                              comma_as_separator, print_always_as_range);
}

/**
 * @brief First id contained in the list
 *
 * The first id of the first range element of the list will
 * be returned. If 'range_list' is nullptr or empty 0 will be
 * returned and 'answer_list' will be filled with an error
 * message.
 *
 * @param range_list RN_Type list, may be an empty handle
 * @param answer_list Pointer to an AN_Type list
 *
 * @return First id or 0
 *
 * @note MT-NOTE: range_list_get_first_id() is MT safe
 *
 * @see #range_list_get_last_id
 */
uint32_t range_list_get_first_id(const ocs::tom::ConstList<ocs::tom::RN> range_list, lList **answer_list) {
   DENTER(RANGE_LAYER);

   uint32_t start = 0;
   const auto range = range_list.front();

   if (range) {
      uint32_t end, step;

      range_get_all_ids(range, &start, &end, &step);
   } else {
      answer_list_add(answer_list, "range_list containes no elements",
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   }
   DRETURN(start);
}

/**
 * @brief range_list_get_first_id() for an untyped caller
 *
 * @param range_list RN_Type list, checked; may be nullptr
 * @param answer_list Pointer to an AN_Type list
 *
 * @return as range_list_get_first_id()
 */
uint32_t range_list_get_first_id(const lList *range_list, lList **answer_list) {
   return range_list_get_first_id(ocs::tom::ConstList<ocs::tom::RN>(range_list), answer_list);
}

/**
 * @brief Returns last id contained in the list
 *
 * The last id of the last range element of the list will be
 * returned. If 'range_list' is nullptr or empty 0 will be
 * returned and 'answer_list' will be filled with an error
 * message.
 *
 * @param range_list RN_Type list, may be an empty handle
 * @param answer_list Pointer to an AN_Type list
 *
 * @return Last id or 0
 *
 * @see #range_list_get_first_id
 */
uint32_t range_list_get_last_id(const ocs::tom::ConstList<ocs::tom::RN> range_list, lList **answer_list) {
   DENTER(RANGE_LAYER);

   uint32_t end = 0;
   const auto range = range_list.back();

   if (range) {
      uint32_t start, step;
      range_get_all_ids(range, &start, &end, &step);
   } else {
      answer_list_add(answer_list, "range_list containes no elements",
                      STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   }
   DRETURN(end);
}

/**
 * @brief range_list_get_last_id() for an untyped caller
 *
 * @param range_list RN_Type list, checked; may be nullptr
 * @param answer_list Pointer to an AN_Type list
 *
 * @return as range_list_get_last_id()
 */
uint32_t range_list_get_last_id(const lList *range_list, lList **answer_list) {
   return range_list_get_last_id(ocs::tom::ConstList<ocs::tom::RN>(range_list), answer_list);
}

/**
 * @brief Return average of all numbers in range
 *
 * The average of all numbers in the range is returned. For an empty
 * range 0 is returned.
 *
 * @param this_list RN_Type list, may be an empty handle
 * @param upperbound This is used as range upperbound if non-0
 *
 * @return the average
 *
 * @note MT-NOTES: range_list_get_average() is MT safe
 */
double range_list_get_average(const ocs::tom::ConstList<ocs::tom::RN> this_list, const uint32_t upperbound) {
   double sum = 0.0;
   int n = 0;

   for (const auto range : this_list) {
      uint32_t min, max, step;

      range_get_all_ids(range, &min, &max, &step);
      if (upperbound != 0) {
         max = std::min(max, upperbound);
      }
      for (uint32_t id = min; id <= max; id += step) {
         sum += id;
         n++;
      }
   }
   return (n > 0) ? (sum / n) : 0;
}

/**
 * @brief range_list_get_average() for an untyped caller
 *
 * @param this_list RN_Type list, checked; may be nullptr
 * @param upperbound This is used as range upperbound if non-0
 *
 * @return as range_list_get_average()
 */
double range_list_get_average(const lList *this_list, uint32_t upperbound) {
   return range_list_get_average(ocs::tom::ConstList<ocs::tom::RN>(this_list), upperbound);
}

/**
 * @brief Makes lists fit as a fiddle
 *
 * After a call to this function 'range_list' fulfills
 * following conditions:
 *    (1) all ids are in ascending order
 *    (2) each id is contained in the list only once
 *    (3) ids are grouped so that a min of range elements exist
 *
 * @code
 * 12-12:7,1-7:1,3-5:2,14-16:2   => 1-7:1,12-16:2
 * @endcode
 *
 * @param range_list RN_Type list; nothing happens for an empty handle
 * @param answer_list Pointer to an AN_Type list
 * @param correct_end if true, range_list is treated as id enumeration
 *
 * @note range_list will be modified
 *
 * @note MT-NOTE: range_list_sort_uniq_compress() is MT safe
 */
void range_list_sort_uniq_compress(const ocs::tom::List<ocs::tom::RN> range_list, lList **answer_list,
                                   const bool correct_end) {
   DENTER(RANGE_LAYER);
   if (range_list) {
      /*
       * Sort the incomming stuff
       */
      range_list.sort<ocs::tom::asc<RN_min>>();

      /*
       * Remove overlapping ranges
       */
      auto tmp_list = ocs::tom::ListPtr<ocs::tom::RN>::create("");
      if (tmp_list) {
         for (auto range1 = range_list.front(); range1; range1 = range1.next()) {
            auto next_range2 = range1.next();

            if (correct_end) {
               range_correct_end(range1);
            }
            while (next_range2) {
               const auto range2 = next_range2;

               next_range2 = range2.next();
               if (correct_end) {
                  range_correct_end(range2);
               }
               if (range_is_overlapping(range1, range2)) {
                  tmp_list->push_back(range_list.extract(range2));
               } else {
                  break;
               }
            }
         }

         /*
          * Insert all removed entries at the correct position
          */
         for (const auto range1 : *tmp_list) {
            uint32_t start1, end1, step1;

            range_get_all_ids(range1, &start1, &end1, &step1);
            for (; start1 <= end1; start1 += step1) {
               ocs_range_list_insert_id(range_list, start1);
            }
         }

         tmp_list.reset();

         /*
          * Join sequenced ranges
          */
         range_list_compress(range_list);
      } else {
         answer_list_add(answer_list, "unable to create range list",
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief range_list_sort_uniq_compress() for an untyped caller
 *
 * @param range_list RN_Type list, checked; may be nullptr
 * @param answer_list Pointer to an AN_Type list
 * @param correct_end if true, range_list is treated as id enumeration
 */
void range_list_sort_uniq_compress(lList *range_list, lList **answer_list, bool correct_end) {
   range_list_sort_uniq_compress(ocs::tom::List<ocs::tom::RN>(range_list), answer_list, correct_end);
}

/**
 * @brief Joins sequenced ranges within a list
 *
 * Consecutive ranges within the list will be joined by this
 * function. Following pre-conditions have to be fulfilled, so
 * that this function works correctly:
 *    (1) ids have to be in ascending order
 *    (2) Only the first/last id of a range may be contained
 *        in the predecessor/successor range
 *
 * @code
 * 1-3:1,4-5:1,6-8:2,8-10:2   => 1-5:1,6-10:2
 * @endcode
 *
 * @param range_list RN_Type list; nothing happens for an empty handle
 *
 * @note 'range_list' will be modified
 *
 * @note MT-NOTE: range_list_compress() is MT safe
 */
void range_list_compress(const ocs::tom::List<ocs::tom::RN> range_list) {
   DENTER(RANGE_LAYER);
   if (range_list) {
      auto next_range1 = range_list.front();
      auto next_range2 = next_range1.next();

      while (next_range1 && next_range2) {
         const auto range1 = next_range1;
         const auto range2 = next_range2;
         uint32_t start1, end1, step1;
         uint32_t start2, end2, step2;

         range_get_all_ids(range1, &start1, &end1, &step1);
         range_get_all_ids(range2, &start2, &end2, &step2);
         if (end1 + step1 == start2 && step1 == step2) {
            end1 = end2;
            step1 = step2;
            range_set_all_ids(range1, start1, end1, step1);
            range_list.erase(range2);
            next_range1 = range1;
         } else if (start1 == end1 && step1 == 1 && end1 == start2 - step2) {
            end1 = end2;
            step1 = step2;
            range_set_all_ids(range1, start1, end1, step1);
            range_list.erase(range2);
            next_range1 = range1;
         } else if (start2 == end2 && step2 == 1 && end1 + step1 == end2) {
            end1 = end2;
            range_set_all_ids(range1, start1, end1, step1);
            range_list.erase(range2);
            next_range1 = range1;
         } else if (start1 == end1 && start2 == end2 && step1 == step2 &&
                    step1 == 1) {
            end1 = start2;
            step1 = end1 - start1;
            range_set_all_ids(range1, start1, end1, step1);
            range_list.erase(range2);
            next_range1 = range1;
         } else {
            next_range1 = range1.next();
         }
         next_range2 = next_range1.next();
      }
   }
   DRETURN_VOID;
}

/**
 * @brief range_list_compress() for an untyped caller
 *
 * @param range_list RN_Type list, checked; may be nullptr
 */
void range_list_compress(lList *range_list) {
   range_list_compress(ocs::tom::List<ocs::tom::RN>(range_list));
}

/**
 * @brief Is id contained in range list?
 *
 * True is returned by this function if 'id' is part of at least
 * one range element of 'range_list'
 *
 * @param range_list RN_Type list, may be an empty handle
 * @param id id
 *
 * @return true or false
 *
 * @note MT-NOTE: range_list_is_id_within() is MT safe
 */
bool range_list_is_id_within(const ocs::tom::ConstList<ocs::tom::RN> range_list, const uint32_t id) {
   DENTER(RANGE_LAYER);
   bool ret = false;

   for (const auto range : range_list) {
      if (range_is_id_within(range, id)) {
         ret = true;
         break;
      }
   }
   DRETURN(ret);
}

/**
 * @brief range_list_is_id_within() for an untyped caller
 *
 * @param range_list RN_Type list, checked; may be nullptr
 * @param id id
 *
 * @return as range_list_is_id_within()
 */
bool range_list_is_id_within(const lList *range_list, uint32_t id) {
   return range_list_is_id_within(ocs::tom::ConstList<ocs::tom::RN>(range_list), id);
}

/**
 * @brief Has id less than x
 *
 * Is at least one id in the "range_list" less than "id"
 *
 * @param range_list RN_Type list, may be an empty handle
 * @param id number
 *
 * @return true or false
 */
bool range_list_containes_id_less_than(const ocs::tom::ConstList<ocs::tom::RN> range_list, const uint32_t id) {
   DENTER(RANGE_LAYER);

   bool ret = false;

   for (const auto range : range_list) {
      if (range_containes_id_less_than(range, id)) {
         ret = true;
         break;
      }
   }
   DRETURN(ret);
}

/**
 * @brief range_list_containes_id_less_than() for an untyped caller
 *
 * @param range_list RN_Type list, checked; may be nullptr
 * @param id number
 *
 * @return as range_list_containes_id_less_than()
 */
bool range_list_containes_id_less_than(const lList *range_list, uint32_t id) {
   return range_list_containes_id_less_than(ocs::tom::ConstList<ocs::tom::RN>(range_list), id);
}

/**
 * @brief Check if id lists containes ids
 *
 * Returns true if "range_list" containes no ids.
 *
 * @param range_list RN_Type list, may be an empty handle
 *
 * @return true or false
 */
bool range_list_is_empty(const ocs::tom::ConstList<ocs::tom::RN> range_list) {
   return range_list_get_number_of_ids(range_list) == 0;
}

/**
 * @brief range_list_is_empty() for an untyped caller
 *
 * @param range_list RN_Type list, checked; may be nullptr
 *
 * @return as range_list_is_empty()
 */
bool range_list_is_empty(const lList *range_list) {
   return range_list_is_empty(ocs::tom::ConstList<ocs::tom::RN>(range_list));
}

/**
 * @brief Test whether the range contains any id strictly less than @p id.
 *
 * The check is equivalent to @c range.min < id — it operates on the stored
 * start value only and does **not** traverse the step grid.  In particular:
 * - Returns @c true even when @p id is greater than @c range.max (only the
 *   start is compared).
 * - Returns @c false when @p id == @c range.min (strictly less than, not <=).
 *
 * @note This is a start-only comparison.  It does NOT test whether @p id
 *       itself is within the range.  Use range_is_id_within() for that.
 *
 * @param range  RN_Type element.  An empty handle returns false.
 * @param id     The exclusive lower-bound threshold.
 * @return       true when @c range.min < @p id, false otherwise.
 *
 * @see range_is_id_within
 * @see range_list_containes_id_less_than
 */
bool range_containes_id_less_than(const ocs::tom::ConstElement<ocs::tom::RN> range, const uint32_t id) {
   DENTER(RANGE_LAYER);

   bool ret = false;

   if (range) {
      uint32_t start, end, step;

      range_get_all_ids(range, &start, &end, &step);
      if (start < id) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Test whether @p id is a member of the range @p range.
 *
 * A value is a member when all three conditions hold:
 * @code
 *   id >= min  &&  id <= max  &&  (id - min) % step == 0
 * @endcode
 *
 * @note The stored @c max field is an inclusive upper bound at the field
 *       level only — it is **not** a guarantee that @c max itself lies on
 *       the step grid.  For example, [1, 6, 2] contains {1, 3, 5};
 *       @c range_is_id_within(r, 6) returns @c false because (6-1)%2 == 1.
 *       Call range_correct_end() first if canonical @c max values are required.
 *
 * @param range  RN_Type element.  An empty handle returns false.
 * @param id     The id to test for membership.
 * @return       true if @p id is a member of @p range, false otherwise.
 *
 * @see range_correct_end
 * @see range_list_is_id_within
 */
bool range_is_id_within(const ocs::tom::ConstElement<ocs::tom::RN> range, const uint32_t id) {
   DENTER(RANGE_LAYER);

   bool ret = false;

   if (range) {
      uint32_t start, end, step;

      range_get_all_ids(range, &start, &end, &step);
      if (id >= start && id <= end && ((id - start) % step) == 0) {
         ret = true;
      }
   }
   DRETURN(ret);
}

/**
 * @brief Remove an id from a range list
 *
 * 'id' will be removed from 'range_list'. A list that becomes empty is
 * freed and the handle emptied.
 *
 * @param range_list in-out: the RN_Type list; nothing happens for an empty handle
 * @param answer_list pointer to a AN_Type list
 * @param id the id to remove
 *
 * @note range_list and answer_list may be modified
 */
void range_list_remove_id(ocs::tom::ListPtr<ocs::tom::RN> &range_list, lList **answer_list, const uint32_t id) {
   DENTER(RANGE_LAYER);

   if (range_list) {
      const auto list = *range_list;
      auto next_range = list.front();

      while (next_range) {
         const auto range = next_range;
         uint32_t start, end, step;

         next_range = range.next();
         range_get_all_ids(range, &start, &end, &step);
         if (id >= start && id <= end && ((id - start) % step) == 0) {
            if ((id == start) && ((id == end) || (id + step > end))) {
               list.erase(range);
               break;
            } else if (id == start) {
               start += step;
               range_set_all_ids(range, start, end, step);
               break;
            } else if (id == end) {
               end -= step;
               range_set_all_ids(range, start, end, step);
               break;
            } else {
               auto new_range = ocs::tom::ElementPtr<ocs::tom::RN>::create();

               if (new_range) {
                  range_set_all_ids(range, start, id - step, step);
                  range_set_all_ids(*new_range, id + step, end, step);
                  list.insert_after(range, std::move(new_range));
               } else {
                  answer_list_add(answer_list, "unable to split range element",
                                  STATUS_ERROR1, ANSWER_QUALITY_ERROR);
               }
               break;
            }
         }
      }
      if (list.empty()) {
         range_list.reset();
      }
   }
   DRETURN_VOID;
}

/**
 * @brief range_list_remove_id() for an untyped caller
 *
 * Also the #range_remove_insert_t callback form.
 *
 * @param range_list pointer to a RN_Type list, checked; nothing happens for nullptr
 * @param answer_list pointer to a AN_Type list
 * @param id the id to remove
 */
void range_list_remove_id(lList **range_list, lList **answer_list, uint32_t id) {
   if (range_list != nullptr) {
      ocs::tom::ListPtr<ocs::tom::RN> list(*range_list);
      range_list_remove_id(list, answer_list, id);
      *range_list = list.release();
   }
}

/**
 * @brief Split a range list
 *
 * The first 'n' ids within 'range_list' will be moved into
 * 'range_list2'. Error messages may be found in 'answer_list'
 *
 * The destination may be created and the source freed when it becomes empty.
 *
 * @param range_list in-out: the RN_Type list (source); nothing happens for an empty handle
 * @param answer_list pointer to an AN_Type list
 * @param range_list2 in-out: the RN_Type list (destination), created if the handle is empty
 * @param n number of ids
 *
 * @note range_list, range_list2, answer_list may be modified
 *
 * @note Known defect CS-3108: the `break` at n == 0 leaves only the inner
 *       loop, so the outer loop continues with the next range and n wraps
 *       around; kept unchanged here.
 */
void range_list_move_first_n_ids(ocs::tom::ListPtr<ocs::tom::RN> &range_list, lList **answer_list,
                                 ocs::tom::ListPtr<ocs::tom::RN> &range_list2, uint32_t n) {
   DENTER(RANGE_LAYER);
   if (range_list) {
      for (const auto range : *range_list) {
         for (uint32_t id = ocs::tom::get<RN_min>(range);
              id <= ocs::tom::get<RN_max>(range);
              id += ocs::tom::get<RN_step>(range)) {
            range_list_insert_id(range_list2, answer_list, id);
            range_list_compress(*range_list2);
            n--;
            if (n == 0) {
               break;
            }
         }
      }
      for (const auto range : *range_list2) {
         for (uint32_t id = ocs::tom::get<RN_min>(range);
              id <= ocs::tom::get<RN_max>(range);
              id += ocs::tom::get<RN_step>(range)) {
            range_list_remove_id(range_list, answer_list, id);
         }
      }
   }
   DRETURN_VOID;
}

/**
 * @brief range_list_move_first_n_ids() for an untyped caller
 *
 * @param range_list pointer to a RN_Type list (source), checked
 * @param answer_list pointer to an AN_Type list
 * @param range_list2 pointer to a RN_Type list (destination), checked
 * @param n number of ids
 */
void range_list_move_first_n_ids(lList **range_list, lList **answer_list, lList **range_list2, uint32_t n) {
   if (range_list != nullptr && range_list2 != nullptr) {
      // both checked before either is taken over
      const ocs::tom::List<ocs::tom::RN> checked_source(*range_list);
      const ocs::tom::List<ocs::tom::RN> checked_destination(*range_list2);
      auto source = ocs::tom::ListPtr<ocs::tom::RN>::from_typed(checked_source);
      auto destination = ocs::tom::ListPtr<ocs::tom::RN>::from_typed(checked_destination);
      range_list_move_first_n_ids(source, answer_list, destination, n);
      *range_list = source.release();
      *range_list2 = destination.release();
   }
}

/**
 * @brief Insert an id into a range list
 *
 * 'id' will be inserted into 'range_list'. The list is created if
 * the handle is empty.
 *
 * @param range_list in-out: the RN_Type list
 * @param answer_list pointer to a AN_Type list; unused since creating the list throws on a lack of memory
 *                    instead of failing
 * @param id new id
 *
 * @note range_list may be modified
 *
 * @note It may be possible that 'id' is multiply contained in 'range_list'
 *       after using this function. Use range_list_compress() or
 *       range_list_sort_uniq_compress() to eliminate them.
 *
 *       MT-NOTE: range_list_insert_id() is MT safe
 *
 * @see #range_list_compress, range_list_sort_uniq_compress()
 */
void range_list_insert_id(ocs::tom::ListPtr<ocs::tom::RN> &range_list, [[maybe_unused]] lList **answer_list,
                          const uint32_t id) {
   DENTER(RANGE_LAYER);
   if (!range_list) {
      range_list = ocs::tom::ListPtr<ocs::tom::RN>::create("task_id_range");
   }
   ocs_range_list_insert_id(*range_list, id);
   DRETURN_VOID;
}

/**
 * @brief range_list_insert_id() for an untyped caller
 *
 * Also the #range_remove_insert_t callback form.
 *
 * @param range_list pointer to a RN_Type list, checked; must not be nullptr
 * @param answer_list pointer to a AN_Type list
 * @param id new id
 */
void range_list_insert_id(lList **range_list, lList **answer_list, uint32_t id) {
   ocs::tom::ListPtr<ocs::tom::RN> list(*range_list);
   range_list_insert_id(list, answer_list, id);
   *range_list = list.release();
}

/**
 * @brief Insert an id into an existing range list
 *
 * The core of range_list_insert_id(), for a list that exists already.
 *
 * @param list the RN_Type list; must not be an empty handle
 * @param id new id
 */
static void ocs_range_list_insert_id(const ocs::tom::List<ocs::tom::RN> list, const uint32_t id) {
   DENTER(RANGE_LAYER);

   bool inserted = false;

   list.sort<ocs::tom::asc<RN_min>>();

   ocs::tom::Element<ocs::tom::RN> range = nullptr;
   ocs::tom::Element<ocs::tom::RN> next_range = nullptr;
   auto prev_range = list.back();
   while (true) {
      next_range = range;
      range = prev_range;
      if (!range) {
         break;
      }

      uint32_t start, end, step;
      uint32_t prev_start, prev_end, prev_step;
      uint32_t next_start, next_end, next_step;

      prev_range = range.prev();
      range_get_all_ids(range, &start, &end, &step);

      /* 1 */
      if (id < end) {
         if (prev_range) {
            continue;
         } else {
            next_range = range;
            range = prev_range;
            prev_range = nullptr;
         }
      }

      if (next_range) {
         range_get_all_ids(next_range, &next_start, &next_end, &next_step);
      }
      if (prev_range) {
         range_get_all_ids(prev_range, &prev_start, &prev_end, &prev_step);
      }

      /* 2 */
      if (next_range && id > next_start) {
         if (((id - next_start) % next_step == 0)) {
            /* id is already part of the range */
            inserted = true;
         } else {
            const uint32_t factor = ((id - next_start) / next_step);
            const uint32_t prev_id = next_start + factor * next_step;
            const uint32_t next_id = next_start + (factor + 1) * next_step;

            range_set_all_ids(next_range, next_start, prev_id, next_step);
            auto new_range1 = ocs::tom::ElementPtr<ocs::tom::RN>::create();
            range_set_all_ids(*new_range1, id, id, 1);
            const auto range1 = list.insert_after(next_range, std::move(new_range1));
            auto new_range2 = ocs::tom::ElementPtr<ocs::tom::RN>::create();
            range_set_all_ids(*new_range2, next_id, next_end, next_step);
            list.insert_after(range1, std::move(new_range2));
            inserted = true;
         }
      } else {
         if ((range && (end == id)) || (next_range && (next_start == id))) {
            /* id is already part of the range */
            inserted = true;
         } else if (range && (end + step == id)) {
            /* 3 */
            end = id;
            range_set_all_ids(range, start, end, step);
            inserted = true;
         } else if (next_range && (next_start - next_step == id)) {
            /* 4 */
            next_start = id;
            range_set_all_ids(next_range, next_start, next_end, next_step);
            inserted = true;
         } else {

            /* 5 */
            auto new_range = ocs::tom::ElementPtr<ocs::tom::RN>::create();
            range_set_all_ids(*new_range, id, id, 1);
            if (range) {
               list.insert_after(range, std::move(new_range));
            } else {
               list.push_front(std::move(new_range));
            }
            inserted = true;
         }
      }
      if (inserted) {
         break;
      }
   }
   if (!inserted) {
      auto new_range = ocs::tom::ElementPtr<ocs::tom::RN>::create();
      range_set_all_ids(*new_range, id, id, 1);
      list.push_back(std::move(new_range));
   }
   DRETURN_VOID;
}

/**
 * @brief Read the (min, max, step) triple from an RN_Type range element.
 *
 * If @p range is an empty handle all three output parameters are set to 0.
 * If any output parameter pointer is nullptr the function returns without
 * writing anything — all three must be non-null for any write to occur.
 *
 * @param range  The RN_Type element to read.  May be an empty handle.
 * @param min    Receives the start of the range; set to 0 for an empty handle.
 * @param max    Receives the end of the range (field value, may be off the step grid);
 *               set to 0 for an empty handle.
 * @param step   Receives the step size; set to 0 for an empty handle.
 *
 * @see range_set_all_ids
 * @see range_correct_end
 */
void range_get_all_ids(const ocs::tom::ConstElement<ocs::tom::RN> range, uint32_t *min, uint32_t *max,
                       uint32_t *step) {
   DENTER(RANGE_LAYER);
   if (min != nullptr && max != nullptr && step != nullptr) {
      if (range) {
         *min = ocs::tom::get<RN_min>(range);
         *max = ocs::tom::get<RN_max>(range);
         *step = ocs::tom::get<RN_step>(range);
      } else {
         *min = *max = *step = 0;
      }
   }
   DRETURN_VOID;
}

/**
 * @brief range_get_all_ids() for an untyped caller
 *
 * @param range The RN_Type element to read, checked; may be nullptr
 * @param min Receives the start of the range
 * @param max Receives the end of the range
 * @param step Receives the step size
 */
void range_get_all_ids(const lListElem *range, uint32_t *min, uint32_t *max, uint32_t *step) {
   range_get_all_ids(ocs::tom::ConstElement<ocs::tom::RN>(range), min, max, step);
}

/**
 * @brief Write (min, max, step) into an RN_Type range element.
 *
 * An empty @p range handle is a silent no-op.
 *
 * @note Step normalisation: when @p min == @p max the step is always stored
 *       as 1, regardless of the @p step argument.  This keeps single-element
 *       ranges in canonical form (e.g. 5-5:3 becomes 5-5:1).
 *
 * @param range  The RN_Type element to modify.  May be an empty handle (no-op).
 * @param min    Start of the range.
 * @param max    End of the range (field value; may be off the step grid —
 *               call range_correct_end() afterwards if canonical storage is needed).
 * @param step   Step size.  Overridden to 1 when @p min == @p max.
 *
 * @see range_get_all_ids
 * @see range_correct_end
 */
void range_set_all_ids(const ocs::tom::Element<ocs::tom::RN> range, const uint32_t min, const uint32_t max,
                       const uint32_t step) {
   DENTER(RANGE_LAYER);
   if (range) {
      ocs::tom::set<RN_min>(range, min);
      ocs::tom::set<RN_max>(range, max);
      if (min != max) {
         ocs::tom::set<RN_step>(range, step);
      } else {
         ocs::tom::set<RN_step>(range, 1);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief range_set_all_ids() for an untyped caller
 *
 * @param range The RN_Type element to modify, checked; nullptr is a no-op
 * @param min Start of the range
 * @param max End of the range
 * @param step Step size
 */
void range_set_all_ids(lListElem *range, uint32_t min, uint32_t max, uint32_t step) {
   range_set_all_ids(ocs::tom::Element<ocs::tom::RN>(range), min, max, step);
}

/**
 * @brief Union set of two range lists
 *
 * All ids contained in 'range_list1' and 'range_list2' will be
 * contained in 'range_list' after a call of this function.
 * Nothing happens if both sources are empty handles.
 *
 * @param range_list in-out: the union set RN_Type list; a list it held before is freed
 * @param answer_list pointer to AN_Type list
 * @param range_list1 first source RN_Type list, may be an empty handle
 * @param range_list2 second source RN_Type list, may be an empty handle
 *
 * @note range_list and answer_list may be modified
 */
void range_list_calculate_union_set(ocs::tom::ListPtr<ocs::tom::RN> &range_list, lList **answer_list,
                                    const ocs::tom::ConstList<ocs::tom::RN> range_list1,
                                    const ocs::tom::ConstList<ocs::tom::RN> range_list2) {
   DENTER(RANGE_LAYER);
   if (range_list1 || range_list2) {
      range_list.reset();

      if (range_list1) {
         range_list = range_list1.clone("");
      } else {
         range_list = range_list2.clone("");
      }

      range_list_sort_uniq_compress(*range_list, answer_list, true);
      if (answer_list_has_error(answer_list)) {
         DTRACE;
         range_list.reset();
         answer_list_add(answer_list, "unable to calculate union set",
                         STATUS_ERROR1, ANSWER_QUALITY_ERROR);
         DRETURN_VOID;
      }

      if (range_list1 && range_list2) {
         for (const auto range2 : range_list2) {
            uint32_t start2, end2, step2;

            range_get_all_ids(range2, &start2, &end2, &step2);
            for (; start2 <= end2; start2 += step2) {
               range_list_insert_id(range_list, answer_list, start2);
            }
         }
         range_list_compress(*range_list);
      }
   }
   DRETURN_VOID;
}

/**
 * @brief range_list_calculate_union_set() for an untyped caller
 *
 * @param range_list pointer to union set RN_Type list, checked; nothing happens for nullptr
 * @param answer_list pointer to AN_Type list
 * @param range_list1 first source RN_Type list, checked; may be nullptr
 * @param range_list2 second source RN_Type list, checked; may be nullptr
 */
void range_list_calculate_union_set(lList **range_list, lList **answer_list,
                                    const lList *range_list1, const lList *range_list2) {
   if (range_list != nullptr) {
      // the sources are checked before the result is taken over
      const ocs::tom::ConstList<ocs::tom::RN> list1(range_list1);
      const ocs::tom::ConstList<ocs::tom::RN> list2(range_list2);
      ocs::tom::ListPtr<ocs::tom::RN> result(*range_list);
      range_list_calculate_union_set(result, answer_list, list1, list2);
      *range_list = result.release();
   }
}

/**
 * @brief Difference set list
 *
 * 'range_list' will contain all ids part of 'range_list1' but not
 * contained in 'range_list2'. Nothing happens if 'range_list1' is an
 * empty handle.
 *
 * @param range_list in-out: the result RN_Type list; a list it held before is freed
 * @param answer_list pointer to AN_Type list
 * @param range_list1 first source RN_Type list, may be an empty handle
 * @param range_list2 second source RN_Type list, may be an empty handle
 */
void range_list_calculate_difference_set(ocs::tom::ListPtr<ocs::tom::RN> &range_list, lList **answer_list,
                                         const ocs::tom::ConstList<ocs::tom::RN> range_list1,
                                         const ocs::tom::ConstList<ocs::tom::RN> range_list2) {
   DENTER(RANGE_LAYER);
   if (range_list1) {
      range_list = range_list1.clone("difference_set range list");

      range_list_sort_uniq_compress(*range_list, answer_list, true);
      if (answer_list_has_error(answer_list)) {
         goto error;
      }

      if (range_list2) {
         for (const auto range2 : range_list2) {
            uint32_t start2, end2, step2;

            range_get_all_ids(range2, &start2, &end2, &step2);
            for (; start2 <= end2; start2 += step2) {
               range_list_remove_id(range_list, answer_list, start2);
               if (answer_list_has_error(answer_list)) {
                  goto error;
               }
            }
         }
         range_list_compress(*range_list);
      }
   }
   DRETURN_VOID;

   error:
   range_list.reset();
   answer_list_add(answer_list, "unable to calculate union set",
                   STATUS_ERROR1, ANSWER_QUALITY_ERROR);
   DRETURN_VOID;
}

/**
 * @brief range_list_calculate_difference_set() for an untyped caller
 *
 * @param range_list pointer to result RN_Type list, checked; nothing happens for nullptr
 * @param answer_list pointer to AN_Type list
 * @param range_list1 first source RN_Type list, checked; may be nullptr
 * @param range_list2 second source RN_Type list, checked; may be nullptr
 */
void range_list_calculate_difference_set(lList **range_list, lList **answer_list,
                                         const lList *range_list1, const lList *range_list2) {
   if (range_list != nullptr) {
      // the sources are checked before the result is taken over
      const ocs::tom::ConstList<ocs::tom::RN> list1(range_list1);
      const ocs::tom::ConstList<ocs::tom::RN> list2(range_list2);
      ocs::tom::ListPtr<ocs::tom::RN> result(*range_list);
      range_list_calculate_difference_set(result, answer_list, list1, list2);
      *range_list = result.release();
   }
}

/**
 * @brief Intersection set
 *
 * 'range_list' will contain all ids which are contained in
 * 'range_list1' and also in 'range_list2'.
 *
 * @param range_list in-out: the result RN_Type list; a list it held before is freed, an empty handle if
 *                   the intersection is empty
 * @param answer_list pointer to AN_Type list; unused since creating lists and elements throws on a lack of
 *                    memory instead of failing
 * @param range_list1 first source RN_Type list, may be an empty handle
 * @param range_list2 second source RN_Type list, may be an empty handle
 */
void range_list_calculate_intersection_set(ocs::tom::ListPtr<ocs::tom::RN> &range_list,
                                           [[maybe_unused]] lList **answer_list,
                                           const ocs::tom::ConstList<ocs::tom::RN> range_list1,
                                           const ocs::tom::ConstList<ocs::tom::RN> range_list2) {
   DENTER(RANGE_LAYER);
   range_list.reset();
   if (range_list1 && range_list2) {
      for (const auto range : range_list1) {
         uint32_t start, end, step;

         range_get_all_ids(range, &start, &end, &step);
         for (; start <= end; start += step) {
            if (range_list_is_id_within(range_list2, start)) {
               if (!range_list) {
                  range_list = ocs::tom::ListPtr<ocs::tom::RN>::create("");
               }
               auto new_range = ocs::tom::ElementPtr<ocs::tom::RN>::create();
               range_set_all_ids(*new_range, start, start, 1);
               range_list->push_back(std::move(new_range));
            }
         }
      }
      range_list_compress(*range_list);
   }
   DRETURN_VOID;
}

/**
 * @brief range_list_calculate_intersection_set() for an untyped caller
 *
 * @param range_list pointer to result RN_Type list, checked; must not be nullptr
 * @param answer_list pointer to AN_Type list
 * @param range_list1 first source RN_Type list, checked; may be nullptr
 * @param range_list2 second source RN_Type list, checked; may be nullptr
 */
void range_list_calculate_intersection_set(lList **range_list, lList **answer_list,
                                           const lList *range_list1, const lList *range_list2) {
   // the sources are checked before the result is taken over
   const ocs::tom::ConstList<ocs::tom::RN> list1(range_list1);
   const ocs::tom::ConstList<ocs::tom::RN> list2(range_list2);
   ocs::tom::ListPtr<ocs::tom::RN> result(*range_list);
   range_list_calculate_intersection_set(result, answer_list, list1, list2);
   *range_list = result.release();
}

/**
 * @brief Appends a range to a dynamic string
 *
 * Appends a range to a dynamic string.
 *
 * @param start min id
 * @param end max id
 * @param step step size
 * @param dyn_taskrange_str dynamic string
 * @param ignore_step ignore step for output
 * @param use_comma_as_separator use a comma instead of '-' and ':' for separation
 * @param print_always_as_range even if the range has only one id
 */
void range_to_dstring(uint32_t start, uint32_t end, int step,
                      dstring *dyn_taskrange_str, int ignore_step,
                      bool use_comma_as_separator, bool print_always_as_range) {
   char tail[256] = "";
   char to_char = '-';
   char step_char = ':';

   if (use_comma_as_separator) {
      to_char = ',';
      step_char = ',';
   }
   if (dyn_taskrange_str->length > 0) {
      sge_dstring_append(dyn_taskrange_str, ",");
   }

   if (start == end && !print_always_as_range) {
      snprintf(tail, sizeof(tail), sge_u32, start);
   } else if (start == end && print_always_as_range) {
      snprintf(tail, sizeof(tail), sge_u32 "%c" sge_u32, start, to_char, end);
   } else if (start + step == end) {
      snprintf(tail, sizeof(tail), sge_u32 "," sge_u32, start, end);
   } else {
      if (ignore_step) {
         snprintf(tail, sizeof(tail), sge_u32 "%c" sge_u32, start, to_char, end);
      } else {
         snprintf(tail, sizeof(tail), sge_u32 "%c" sge_u32 "%c%d", start, to_char, end, step_char, step);
      }
   }
   sge_dstring_append(dyn_taskrange_str, tail);
}

/**
 * @brief Parse one range out of its textual form
 *
 * Accepts `n`, `min-max`, `min-max:step` and the open forms `-n` (meaning
 * `1-n`) and `n-` (meaning `n-`#RANGE_INFINITY). The literal `UNDEFINED`
 * yields no element at all.
 *
 * @param[out] answer_list receives the syntax error; may be nullptr
 * @param rstr the text to parse
 * @param step_allowed whether a `:step` suffix may appear
 * @param inf_allowed whether an open upper bound may appear
 * @return the new range (`RN_Type`), or an empty handle for `UNDEFINED`, for an
 *         open bound that is not allowed and on a parse error
 *
 * @note MT-NOTE: range_parse_from_string() is MT safe
 */
ocs::tom::ElementPtr<ocs::tom::RN>
range_parse_from_string(lList **answer_list, const char *rstr, const int step_allowed, const int inf_allowed) {
   DENTER(TOP_LAYER);

   const char *old_str;
   char *dptr;
   uint32_t rmin, rmax, ldummy, step = 1;
   char msg[MAX_STRING_SIZE];

   old_str = rstr;

   if (!strcasecmp(rstr, "UNDEFINED")) {
      DRETURN(nullptr);
   }
   auto r = ocs::tom::ElementPtr<ocs::tom::RN>::create();

   rmin = rmax = 0;
   if (rstr[0] == '-') {
      /* rstr e.g. is "-<n>" ==> min=1 */
      rmin = 1;
      rstr++;
      if (*rstr == '\0') {
         if (inf_allowed) {
            /* rstr is just "-" <==> "1-inf" */
            ocs::tom::set<RN_min>(r, rmin);
            ocs::tom::set<RN_max>(r, RANGE_INFINITY);
            DRETURN(r);
         } else {
            DRETURN(nullptr);
         }
      }
   }

   /* rstr should point to a decimal now */
   ldummy = strtol(rstr, &dptr, 10);
   if ((ldummy == 0) && (rstr == dptr)) {
      snprintf(msg, sizeof(msg), MSG_GDI_INITIALPORTIONSTRINGNODECIMAL_S, rstr);
      answer_list_add(answer_list, msg, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      DRETURN(nullptr);
   }

   if (rmin != 0) {
      /* rstr is "-<n>" and ldummy contains <n>
       * dptr poits right after <n>.
       */
      if (*dptr != '\0' || (step_allowed && *dptr != ':')) {
         snprintf(msg, sizeof(msg), MSG_GDI_RANGESPECIFIERWITHUNKNOWNTRAILER_SS, old_str, rstr);
         answer_list_add(answer_list, msg, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
         DRETURN(nullptr);
      }
      /* <n> is the max-value */
      rmax = ldummy;

   } else {     /* rmin==0) */
      /*
       ** rstr is "<n>" or "<n>-..." and ldummy contains <n>
       ** dptr poits right after <n>.
       */
      if (*dptr == '\0') {
         /* rstr is just "<n>" */
         rmin = ldummy;
         rmax = ldummy;
      } else {
         /* rstr should be "<n>-..." */
         if (!
                 (*dptr == '-' || isdigit((int) *(dptr + 1)) || *(dptr + 1) == '\0'
                  || (step_allowed && *dptr == ':'))) {
            /* ... but isn't */
            snprintf(msg, sizeof(msg), MSG_GDI_RANGESPECIFIERWITHUNKNOWNTRAILER_SS, old_str, dptr);
            answer_list_add(answer_list, msg, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
            DRETURN(nullptr);
         } else {
            /* it is. set min to ldummy. now, what's after the - */
            rmin = ldummy;
            rstr = dptr + 1;
            if (*rstr == '\0') {
               /* the range string was "<n>-" <==> "ldummy-inf" */
               if (inf_allowed) {
                  rmax = RANGE_INFINITY;
               } else {
                  DRETURN(nullptr);
               }
            } else {
               /* the trailer should contain a decimal - go for it */
               ldummy = strtol(rstr, &dptr, 10);
               if ((ldummy == 0) && (rstr == dptr)) {
                  snprintf(msg, sizeof(msg), MSG_GDI_INITIALPORTIONSTRINGNODECIMAL_S, rstr);
                  answer_list_add(answer_list, msg, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  DRETURN(nullptr);
               }

               if (!(*dptr == '\0' || (step_allowed && *dptr == ':'))) {
                  snprintf(msg, sizeof(msg), MSG_GDI_RANGESPECIFIERWITHUNKNOWNTRAILER_SS, rstr, dptr);
                  answer_list_add(answer_list, msg, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                  DRETURN(nullptr);
               }
               /* finally, we got the max-value in ldummy */
               rmax = ldummy;

               if (step_allowed && *dptr && *dptr == ':') {
                  const double epsilon = 1.0E-12;
                  double dbldummy;

                  rstr = dptr + 1;
                  dbldummy = strtod(rstr, &dptr);
                  ldummy = dbldummy;

                  if (dbldummy > 0) {
                     if (( dbldummy - ldummy > epsilon) ||
                        ((ldummy == 0) && (rstr == dptr))) {
                        snprintf(msg, sizeof(msg), MSG_GDI_INITIALPORTIONSTRINGNODECIMAL_S, rstr);
                        answer_list_add(answer_list, msg, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                        DRETURN(nullptr);
                     }
                  }
                  else if (dptr == rstr) {
                     snprintf(msg, sizeof(msg), MSG_GDI_INITIALPORTIONSTRINGNODECIMAL_S, rstr);
                     answer_list_add(answer_list, msg, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     DRETURN(nullptr);
                  }
                  else {
                     snprintf(msg, sizeof(msg), SFNMAX, MSG_GDI_NEGATIVSTEP );
                     answer_list_add(answer_list, msg, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     DRETURN(nullptr);
                  }

                  if (*dptr != '\0') {
                     snprintf(msg, sizeof(msg), MSG_GDI_RANGESPECIFIERWITHUNKNOWNTRAILER_SS, rstr, dptr);
                     answer_list_add(answer_list, msg, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
                     DRETURN(nullptr);
                  }
                  /* finally, we got the max-value in ldummy */
                  step = ldummy;

               }
            }   /* if (*rstr == '\0') -- else clause */
         }      /* if (*dptr != '-') -- else clause */
      } /* if (*dptr == '\0') -- else clause */
   }    /* if (r->min != 0) -- else clause */

   /* We're ready? Not quite! We still have to check whether min<=max */
   if (rmin > rmax) {
      ldummy = rmax;
      rmax = rmin;
      rmin = ldummy;
   }

   ocs::tom::set<RN_min>(r, rmin);
   ocs::tom::set<RN_max>(r, rmax);
   ocs::tom::set<RN_step>(r, step);

   /* Ughhhh! Done ... */

   DRETURN(r);
}

/**
 * @brief parse range string and return counters
 *
 * @param[in] value        string containing range specification
 * @param[in] step_allowed may the range specification contain a step?
 * @param[out] start       range start
 * @param[out] end         range end
 * @param[out] step        range step size
 * @return true if the input string is a range, else false
 */
bool
range_parse_get_ids(const char *value, const int step_allowed, uint32_t &start, uint32_t &end, uint32_t &step) {
   bool ret = true;
   const auto range = range_parse_from_string(nullptr, value, step_allowed, 0);

   if (!range) {
      // string didn't contain a valid range
      ret = false;
   } else {
      start = ocs::tom::get<RN_min>(*range);
      end = ocs::tom::get<RN_max>(*range);
      step = ocs::tom::get<RN_step>(*range);
   }

   return ret;
}

/**
 * @brief Parse a comma separated list of ranges into a range list
 *
 * The literal `UNDEFINED` yields no list, and may not be followed by further
 * ranges. On a syntax error the part parsed so far is freed.
 *
 * @param[in,out] this_list receives the parsed list, merged into a list it holds already; an empty
 *                          handle on a syntax error
 * @param[out] answer_list receives the syntax error instead of the function
 *                         exiting; must point at a nullptr list
 * @param string the text to parse
 * @param just_parse true to validate only, without building a list
 * @param step_allowed whether a `:step` suffix may appear
 * @param inf_allowed whether an open upper bound may appear
 * @return true when the whole string was a valid range list
 *
 * @note MT-NOTE: range_list_parse_from_string() is MT safe
 */
bool
range_list_parse_from_string(ocs::tom::ListPtr<ocs::tom::RN> &this_list, lList **answer_list,
                             const char *string, const bool just_parse,
                             const bool step_allowed, const bool inf_allowed) {
   DENTER(TOP_LAYER);

   const char *s;
   bool undefined = false, first = true;
   struct saved_vars_s *context = nullptr;

   for (s = sge_strtok_r(string, RANGE_SEPARATOR_CHARS, &context);
        s; s = sge_strtok_r(nullptr, RANGE_SEPARATOR_CHARS, &context)) {
      if (!first && undefined) {
         /* first was undefined - no more ranges allowed */
         ERROR(SFNMAX, MSG_GDI_UNEXPECTEDRANGEFOLLOWINGUNDEFINED);
         sge_free_saved_vars(context);
         answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                         ANSWER_QUALITY_ERROR);
         this_list.reset();
         DRETURN(false);
      }

      auto range = range_parse_from_string(answer_list, s, step_allowed, inf_allowed);

      if (!range) {
         if (first) {
            undefined = true;
         } else {
            /* second range may not be undefined ! */
            ERROR(SFNMAX, MSG_GDI_UNEXPECTEDUNDEFINEDFOLLOWINGRANGE);
            sge_free_saved_vars(context);
            answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX,
                            ANSWER_QUALITY_ERROR);
            this_list.reset();
            DRETURN(false);
         }
      } else if (!just_parse) {
         expand_range_list(std::move(range), this_list);
      }
      // a range that was only parsed is freed with its handle

      first = false;
   }

   sge_free_saved_vars(context);
   DRETURN(true);
}

/**
 * @brief range_list_parse_from_string() for an untyped caller
 *
 * @param[in,out] this_list receives the parsed list, checked; pass nullptr to only check the syntax
 * @param[out] answer_list receives the syntax error
 * @param string the text to parse
 * @param just_parse true to validate only, without building a list
 * @param step_allowed whether a `:step` suffix may appear
 * @param inf_allowed whether an open upper bound may appear
 * @return as range_list_parse_from_string()
 */
bool range_list_parse_from_string(lList **this_list, lList **answer_list, const char *string, bool just_parse,
                                  bool step_allowed, bool inf_allowed) {
   // a list built without a target is not handed out, the handle frees it
   ocs::tom::ListPtr<ocs::tom::RN> list(this_list != nullptr ? *this_list : nullptr);
   const bool ret = range_list_parse_from_string(list, answer_list, string, just_parse, step_allowed, inf_allowed);
   if (this_list != nullptr) {
      *this_list = list.release();
   }
   return ret;
}
