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
 * @brief Parsing and printing the numeric values users write
 *
 * A value may carry a unit (`4G`, `1:30:00`) or be the word `infinity`, and
 * which forms are accepted depends on the resource's type. These are the
 * conversions in both directions.
 *
 * @see sge_ulong.h
 */

#include <math.h>
#include <cfloat>
#include <ctime>
#include <cstring>
#include <climits>

#include "uti/sge.h"               // INFINITY_STR
#include "uti/sge_dstring.h"
#include "uti/sge_log.h"
#include "uti/sge_parse_num_par.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_string.h"
#include "uti/sge_time.h"

#include "sgeobj/sge_answer.h"
#include "sgeobj/sge_ulong.h"
#include "sgeobj/sge_centry.h"
#include "sgeobj/msg_sgeobjlib.h"

#include <cinttypes>

/// Debug layer the value parsing and printing traces are written to
#define ULONG_LAYER TOP_LAYER

/**
 * @brief Render a double as the literal `infinity` when it is infinite
 *
 * Nothing is written for a finite value, so the type specific printers call
 * this first and only render the number themselves when it returns false.
 *
 * @param value the value to render
 * @param[out] string receives the text, appended
 * @return true when the value was infinite and something was written
 *
 * @note MT-NOTE: double_print_infinity_to_dstring() is MT safe
 */
bool double_print_infinity_to_dstring(double value, dstring *string) {
   DENTER(ULONG_LAYER);

   bool ret = true;

   if (string != nullptr) {
      if (value == DBL_MAX) {
         // CS-2318: display unlimited as the canonical uppercase INFINITY_STR,
         // consistent with the stored token (cqueue defaults, default_duration) and
         // qconf/JSON output. Parsing stays case-insensitive, so old input works.
         sge_dstring_append(string, INFINITY_STR);
      } else {
         ret = false;
      }
   }
   DRETURN(ret);
}

/*
* NOTES
*     MT-NOTE: double_print_time_to_dstring() is MT safe
*/
/**
 * @brief Render a double as a time span as `h:mm:ss`
 *
 * @param value the value to render
 * @param[out] string receives the text, appended
 * @return true when something was written
 *
 * @note MT-NOTE: double_print_time_to_dstring() is MT safe
 */
bool double_print_time_to_dstring(double value, dstring *string) {
   return double_print_time_to_dstring(value, string, false);
}

/**
 * @brief Render a double as a time span, optionally with microseconds
 *
 * @param value the value to render
 * @param[out] string receives the text, appended
 * @param with_microseconds true to append the fractional part
 * @return true when something was written
 *
 * @note MT-NOTE: double_print_time_to_dstring() is MT safe
 */
bool double_print_time_to_dstring(double value, dstring *string, bool with_microseconds) {
   DENTER(ULONG_LAYER);

   bool ret = true;

   if (string != nullptr) {
      if (!double_print_infinity_to_dstring(value, string)) {
         const uint32_t minute_in_seconds = 60;
         const uint32_t hour_in_seconds = minute_in_seconds * 60;
         const uint32_t day_in_seconds = hour_in_seconds * 24;
         int seconds, minutes, hours, days;

         days = value / day_in_seconds;
         seconds = value - days * day_in_seconds;
         hours = seconds / hour_in_seconds;
         seconds -= hours * hour_in_seconds;
         minutes = seconds / minute_in_seconds;
         seconds -= minutes * minute_in_seconds;

         if (days > 0) {
            sge_dstring_sprintf_append(string, "%d:%02d:%02d:%02d",
                                       days, hours, minutes, seconds);
         } else {
            sge_dstring_sprintf_append(string, "%2.2d:%2.2d:%2.2d",
                                       hours, minutes, seconds);
         }

         if (with_microseconds) {
            int microseconds = int((value - floor(value)) * 1000000.0);
            sge_dstring_sprintf_append(string, ".%06d", microseconds);
         }
      }
   }
   DRETURN(ret); 
}

/*
* NOTES
*     MT-NOTE: double_print_memory_to_dstring() is MT safe
*/
/**
 * @brief Render a double as a memory size with its unit suffix, e.g. `4.000G`
 *
 * @param value the value to render
 * @param[out] string receives the text, appended
 * @return true when something was written
 *
 * @note MT-NOTE: double_print_memory_to_dstring() is MT safe
 */
bool double_print_memory_to_dstring(double value, dstring *string) {
   DENTER(ULONG_LAYER);

   bool ret = true;

   if (string != nullptr) {
      if (!double_print_infinity_to_dstring(value, string)) {
         const double kilo_byte = 1024;
         const double mega_byte = kilo_byte * 1024;
         const double giga_byte = mega_byte * 1024;
         double absolute_value = fabs(value); 
         char unit = '\0';

         if (absolute_value >= giga_byte) {
            value /= giga_byte;
            unit = 'G';
         } else if (absolute_value >= mega_byte) {
            value /= mega_byte;
            unit = 'M';
         } else if (absolute_value >= kilo_byte) {
            value /= kilo_byte;
            unit = 'K';
         }
         if (unit != '\0') {
            sge_dstring_sprintf_append(string, "%.3f%c", value, unit);
         } else {
            sge_dstring_sprintf_append(string, "%.3f", absolute_value);
         }
      } 
   }
   DRETURN(ret);
}

/**
 * @brief Print a double into a dstring as an int
 *
 * Print a double into a dstring as an int.
 *
 * @param value the value to print
 * @param string the dstring to receive the value
 *
 * @return returns false if value is out of range for an int
 *
 * @note MT-NOTE: double_print_int_to_dstring() is MT safe
 */
bool double_print_int_to_dstring(double value, dstring *string) {
   DENTER(ULONG_LAYER);

   bool ret = true;

   if (string != nullptr) {
      if (!double_print_infinity_to_dstring(value, string)) {
         const double min_as_dbl = INT_MIN;
         const double max_as_dbl = INT_MAX;

         if (value > max_as_dbl || value < min_as_dbl) {
            sge_dstring_append(string, "integer_overflow");
            DRETURN(false);
         }
     
         sge_dstring_sprintf_append(string, "%d", (int)value);
      } 
   }
   DRETURN(ret);
}

/**
 * @brief Print a double into a dstring
 *
 * Print a double into a dstring.
 *
 * @param value the value to print
 * @param string the dstring to receive the value
 *
 * @return returns false if something goes wrong
 *
 * @note MT-NOTE: double_print_to_dstring() is MT safe
 */
bool double_print_to_dstring(double value, dstring *string) {
   DENTER(ULONG_LAYER);

   bool ret = true;

   if (string != nullptr) {
      if (!double_print_infinity_to_dstring(value, string)) {
         sge_dstring_sprintf_append(string, "%f", value);
      } 
   }
   DRETURN(ret);
}

/**
 * @brief Render a double the way its resource type is written
 *
 * @param value the value to render
 * @param[out] string receives the text, appended
 * @param type the resource type deciding the format
 * @return true when something was written
 */
bool double_print_to_dstring(double value, dstring *string, ocs::CEntry::Type type) {
   switch (type) {
      case ocs::CEntry::Type::TIME:
         return double_print_time_to_dstring(value, string);
      case ocs::CEntry::Type::MEM:
         return double_print_memory_to_dstring(value, string);
      default:
         return double_print_to_dstring(value, string);
   }
}

/**
 * @brief Parse a date/time specifier into seconds since the epoch.
 *
 * Parses a string of the form [[CC]YY]MMDDhhmm[.SS] (as accepted by the
 * qsub/qalter "-a" option, qacct "-b"/"-e" and DRMAA start time). The input is
 * attacker-controlled and is copied into a fixed-size stack buffer of
 * sizeof(stringT) bytes, so its length must be strictly less than the buffer
 * capacity to leave room for the terminating NUL. Oversized, empty or malformed
 * input is rejected. MT-NOTE: this function is MT safe.
 *
 * @param[out] this_ulong  parsed time in seconds since the epoch, or -1 (cast to
 *                         uint32_t) on any error
 * @param[out] answer_list error answer on failure; if nullptr the message is
 *                         printed to stderr instead
 * @param[in]  string      the date/time string to parse
 * @return true on success, false on error (NULL/empty/too long/malformed)
 */
bool ulong_parse_date_time_from_string(uint32_t *this_ulong,
                                       lList **answer_list, const char *string) {
   DENTER(TOP_LAYER);

   int i;
   int year_fieldlen=2;
   const char *seconds;
   const char *non_seconds;
   stringT tmp_str;
   stringT inp_date_str;

   time_t gmt_secs;
   struct tm res;
   struct tm *tmp_timeptr,timeptr;
   struct saved_vars_s *context = nullptr;

   memset(tmp_str, 0, sizeof(tmp_str));

   if (!string || string[0] == '\0') {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_NODATE);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;   
      DRETURN(false);
   }

   // Reject before the strcpy below if the string does not fit together with
   // its NUL terminator: a string of length sizeof(stringT) would need one
   // extra byte for the terminator, so the bound is '>=', not '>' (CS-2349,
   // CWE-193/CWE-787 off-by-one stack overflow).
   if (strlen(string) >= sizeof(stringT)) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_STARTTIMETOOLONG);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;
      DRETURN(false);
   }

   sge_strlcpy(inp_date_str, string, sizeof(inp_date_str));
   non_seconds=sge_strtok_r(inp_date_str, ".", &context);
   seconds=sge_strtok_r(nullptr, ".", &context);

   if (seconds) {
      i=strlen(seconds);
   } else {
      i = 0;
   }

   if ((i != 0) && (i != 2)) {
      sge_free_saved_vars(context);
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_INVALIDSECONDS);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;
      DRETURN(false);
   }

   i=strlen(non_seconds);

   if ((i != 8) && (i != 10) && (i != 12)) {
      sge_free_saved_vars(context);
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_INVALIDHOURMIN);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;
      DRETURN(false);
   }

   memset((char *)&timeptr, 0, sizeof(timeptr));

   if (i==12) {
      year_fieldlen=4;
   }

   if (i>=10) {
      memset(tmp_str, 0, sizeof(tmp_str));
      memcpy(tmp_str, non_seconds, year_fieldlen);
      timeptr.tm_year=atoi(tmp_str);
      if (i==12) {
         timeptr.tm_year -= 1900;
      }
      else {
         /* the date is before 1970, thus we assume, that
            20XX is ment. This works only till 2069, but
            that should be sufficent for now */
         if (timeptr.tm_year < 70)
            timeptr.tm_year += 100;
      }
      non_seconds+=year_fieldlen;
   } else {
      gmt_secs = time(nullptr);
      tmp_timeptr=localtime_r(&gmt_secs, &res);
      timeptr.tm_year=tmp_timeptr->tm_year;
   }

   memset(tmp_str, 0, sizeof(tmp_str));
   memcpy(tmp_str, non_seconds, 2);
   timeptr.tm_mon=atoi(tmp_str)-1;/* 00==Jan, we don't like that do we */
   if ((timeptr.tm_mon>11)||(timeptr.tm_mon<0)) {
      sge_free_saved_vars(context);
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_INVALIDMONTH);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;
      DRETURN(false);
   }

   non_seconds+=2;

   memset(tmp_str, 0, sizeof(tmp_str));
   memcpy(tmp_str, non_seconds, 2);
   timeptr.tm_mday=atoi(tmp_str);
   non_seconds+=2;

   /* yea, we should do it by mon ths */
   if ((timeptr.tm_mday > 31) || (timeptr.tm_mday < 1)) {
      /* actually mktime() should frigging do it */
      sge_free_saved_vars(context);
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_INVALIDDAY);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;
      DRETURN(false);
   }

   memset(tmp_str, 0, sizeof(tmp_str));
   memcpy(tmp_str, non_seconds, 2);
   timeptr.tm_hour=atoi(tmp_str);
   non_seconds+=2;

   if ((timeptr.tm_hour > 23) || (timeptr.tm_hour < 0)) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_INVALIDHOUR);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;
      DRETURN(false);
   }

   memset(tmp_str, 0, sizeof(tmp_str));
   memcpy(tmp_str, non_seconds, 2);
   timeptr.tm_min=atoi(tmp_str);

   if ((timeptr.tm_min > 59)||(timeptr.tm_min < 0)) {
      sge_free_saved_vars(context);
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_INVALIDMINUTE);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;
      DRETURN(false);
   }

   if (seconds) {
      timeptr.tm_sec=atoi(seconds);
   }
   if ((timeptr.tm_sec>59)||(timeptr.tm_mday<0)) {
      sge_free_saved_vars(context);
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_INVALIDSECOND);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;
      DRETURN(false);
   }

   /*
   ** for daylight saving corrections
   */
   timeptr.tm_isdst = -1;

   gmt_secs=mktime(&timeptr);

   DPRINTF("mktime returned: %ld\n",gmt_secs);

   if (gmt_secs < 0) {
      sge_free_saved_vars(context);
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_PARSE_NODATEFROMINPUT);
      if (answer_list) {
         answer_list_add(answer_list, SGE_EVENT, 
                         STATUS_EUNKNOWN, ANSWER_QUALITY_ERROR);
      } else {
         fprintf(stderr,"\n%s\n", SGE_EVENT);
      }
      *this_ulong = -1;
      DRETURN(false);
   }

   sge_free_saved_vars(context);

   *this_ulong = gmt_secs;
   DRETURN(true);
}

/**
 * @brief Parse a complex entry type name, e.g. `INT` or `MEMORY`
 *
 * @param[out] this_ulong receives the parsed value
 * @param[out] answer_list receives the message naming the bad text
 * @param string the text to parse
 * @return true when the text was understood
 */
bool ulong_parse_centry_type_from_string(uint32_t *this_ulong,
                                         lList **answer_list, const char *string) {
   DENTER(TOP_LAYER);

   bool ret = true;
   int i;
   *this_ulong = 0;
   for (i = static_cast<int>(ocs::CEntry::Type::FIRST); i <= static_cast<int>(ocs::CEntry::Type::CE_LAST); i++) {
      if (strcasecmp(string, map_type2str(static_cast<ocs::CEntry::Type>(i))) == 0) {
         *this_ulong = i;
         break;
      }
   }
   if (strcasecmp(string, map_type2str(ocs::CEntry::Type::RSMAP)) == 0) {
      *this_ulong = i;
   }
   if (*this_ulong == 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALID_CENTRY_TYPE_S, string);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Parse a complex entry relational operator, e.g. `>=`
 *
 * @param[out] this_ulong receives the parsed value
 * @param[out] answer_list receives the message naming the bad text
 * @param string the text to parse
 * @return true when the text was understood
 */
bool ulong_parse_centry_relop_from_string(uint32_t *this_ulong,
                                          lList **answer_list, const char *string) {
   DENTER(TOP_LAYER);

   bool ret = true;
   int i;
   *this_ulong = 0;
   for (i = CMPLXEQ_OP; i <= CMPLXNE_OP; i++) {
      if (!strcasecmp(string, map_op2str(i))) {
         *this_ulong = i;
         break;
      }
   }
   if (*this_ulong == 0) {
      answer_list_add_sprintf(answer_list, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR,
                              MSG_INVALID_CENTRY_RELOP_S, string);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Parse a plain unsigned integer
 *
 * @param[out] this_ulong receives the parsed value
 * @param[out] answer_list receives the message naming the bad text
 * @param string the text to parse
 * @return true when the text was understood
 */
bool ulong_parse_from_string(uint32_t *this_ulong,
                             lList **answer_list, const char *string) {
   DENTER(TOP_LAYER);

   bool ret = true;
      
   if (this_ulong != nullptr && string != nullptr) {
      if (!parse_ulong_val(nullptr, this_ulong, ocs::CEntry::Type::INT, string, nullptr, 0)) {
         answer_list_add(answer_list, MSG_PARSE_INVALID_ID_MUSTBEUINT,
                         STATUS_ESEMANTIC, ANSWER_QUALITY_ERROR);
         ret = false;
      }
   }
   DRETURN(ret);
}


/**
 * @brief Parse a delimited list of unsigned integers
 *
 * @param[out] this_list receives the parsed values
 * @param[out] answer_list receives the message naming the bad text
 * @param string the text to parse
 * @param delimitor the characters separating two values
 * @return true when the whole text was understood
 */
bool ulong_list_parse_from_string(lList **this_list, lList **answer_list,
                                  const char *string, const char *delimitor) {
   DENTER(TOP_LAYER);

   bool ret = true;
                                
   if (this_list != nullptr && string != nullptr && delimitor != nullptr) {
      struct saved_vars_s *context = nullptr;
      const char *token;
            
      token = sge_strtok_r(string, delimitor, &context);
      while (token != nullptr) {
         uint32_t value;

         ret = ulong_parse_from_string(&value, answer_list, token);
         if (ret) {
            lAddElemUlong(this_list, ULNG_value, value, ULNG_Type);
         } else {
            break;
         }
         token = sge_strtok_r(nullptr, delimitor, &context);
      }
      sge_free_saved_vars(context);
   }        
   DRETURN(ret);
}

/**
 * @brief Parse a POSIX job priority
 *
 * The accepted range is -1023 to 1024.
 *
 * @param[out] answer_list receives the message naming the bad value
 * @param[out] valp receives the parsed value
 * @param priority_str the text to parse
 * @return true when the text was understood
 */
bool ulong_parse_priority(lList **answer_list, int *valp, const char *priority_str) {
   DENTER(TOP_LAYER);

   bool ret = true;
   char *s;

   *valp = strtol(priority_str, &s, 10);
   if ((char*)valp == s || *valp > 1024 || *valp < -1023) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ULNG_INVALIDPRIO_I, (int) *valp);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}

/**
 * @brief Parse the number of cores a binding request asks for
 *
 * A negative value is rejected.
 *
 * @param[out] answer_list receives the message naming the bad value
 * @param[out] valp receives the parsed value
 * @param bamount_str the text to parse
 * @return true when the text was understood
 */
bool ulong_parse_binding_amount(lList **answer_list, int *valp, const char *bamount_str) {
   DENTER(TOP_LAYER);

   bool ret = true;
   char *s;

   *valp = strtol(bamount_str, &s, 10);
   if ((char*)valp == s || *valp < 0) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ULNG_INVALID_BAMOUNT_I, (int) *valp);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}

/* DG: TODO: add ADOC */
/**
 * @brief Parse a value that may carry a unit or be `infinity`
 *
 * Accepts the memory and time forms as well as a plain number.
 *
 * @param[out] this_ulong receives the parsed value
 * @param[out] answer_list receives the message naming the bad text
 * @param string the text to parse
 * @return true when the text was understood
 */
bool ulong_parse_value_from_string(uint32_t *this_ulong,
                                   lList **answer_list, const char *string) {
   DENTER(TOP_LAYER);

   bool ret = true;
   char *s;
   
   *this_ulong = strtol(string, &s, 10);
   if (string == s) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, SFNMAX, MSG_ULNG_INVALID_VALUE);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }

   DRETURN(ret);
}

/**
 * @brief Parse the array task concurrency limit
 *
 * A negative value is rejected.
 *
 * @param[out] answer_list receives the message naming the bad value
 * @param[out] valp receives the parsed value
 * @param task_concurrency_str the text to parse
 * @return true when the text was understood
 */
bool ulong_parse_task_concurrency(lList **answer_list, int *valp, const char *task_concurrency_str) {
   DENTER(TOP_LAYER);

   bool ret = true;
   char *s;

   *valp = strtol(task_concurrency_str, &s, 10);
   if (task_concurrency_str == s || *valp < 0) {
      snprintf(SGE_EVENT, SGE_EVENT_SIZE, MSG_ULNG_INVALID_TASK_CONCURRENCY_I, (int) *valp);
      answer_list_add(answer_list, SGE_EVENT, STATUS_ESYNTAX, ANSWER_QUALITY_ERROR);
      ret = false;
   }
   DRETURN(ret);
}
