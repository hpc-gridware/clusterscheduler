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
 ************************************************************************/
/*___INFO__MARK_END__*/
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <sys/time.h>

#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"

#include "qmaster_heartbeat.h"
#include "msg_daemons_common.h"

static int sge_testmode_timeout_value = 0;
static int sge_testmode_timeout_at_heartbeat = 0;

/*--------------------------------------------------------------
 * Name:   get_qmaster_heartbeat
 * Descr:  get number found in qmaster heartbeat file
 * Return: > 0 number found in given heartbeat file 
 *         -1   can't open file 
 *         -2   can't read entry
 *         -3   read timeout
 *         -4   fclose error
 *-------------------------------------------------------------*/
int get_qmaster_heartbeat(const char *file, time_t read_timeout) {
   FILE *fp   = nullptr;
   int hb     = 0; 
   struct timeval start_time;
   struct timeval end_time;
   time_t read_time;

   DENTER(TOP_LAYER);

   if (file == nullptr) {
      ERROR(SFNMAX, MSG_HEART_NO_FILENAME);
      DRETURN(-1);
   }

   gettimeofday(&start_time,nullptr);

   fp = fopen(file, "r");
   if (!fp) {
      ERROR(MSG_HEART_CANNOTOPEN_SS, file, strerror(errno));
      DRETURN(-1);
   }

   if (fscanf(fp, "%d", &hb) != 1) {
      FCLOSE(fp);
      ERROR(MSG_HEART_CANNOT_READ_FILE_S, strerror(errno));
      DRETURN(-2);
   }

   FCLOSE(fp);

   /* This is only for testsuite testing */
   if (sge_testmode_timeout_value > 0 && hb == sge_testmode_timeout_at_heartbeat ) {
      sleep(sge_testmode_timeout_value);
   }

   gettimeofday(&end_time,nullptr);
   read_time = end_time.tv_sec - start_time.tv_sec;
   if (read_time > read_timeout) {
      ERROR(MSG_HEART_READ_TIMEOUT_S, file);
      DRETURN(-3);
   }

   DRETURN(hb);
FCLOSE_ERROR:
   ERROR(MSG_HEART_CLOSE_ERROR_SS, file, strerror(errno));
   DRETURN(-4);
}

/*--------------------------------------------------------------
 * Name:   inc_qmaster_heartbeat
 * Descr:  increment number found in qmaster heartbeat file
 *         wrap at 99999
 *         if file doesn't exist create it
 * Return: 0 if operation was successful
 *           (if beat_value != nullptr it will contain
 *            the written heartbeat value)
 *
 * Errors:
 *           -1: can't open file
 *           -2: seek error
 *           -3: write error
 *           -4: write took longer than write_timeout seconds
 *           -5: fclose error
 *
 * Notice:   if return != 0 then beat_value is not written !
 *-------------------------------------------------------------*/
int inc_qmaster_heartbeat(const char *file, time_t write_timeout , int* beat_value) {

   FILE *fp = nullptr;
   int hb   = 1;
   struct timeval start_time;
   struct timeval end_time;
   time_t write_time;

   DENTER(TOP_LAYER);

   if (file == nullptr) {
      ERROR(SFNMAX, MSG_HEART_NO_FILENAME);
      DRETURN(-1);
   }

   gettimeofday(&start_time,nullptr);

   /* Try to open heartbeat file */
   fp = fopen(file, "r+");

   /* If we can't open the file we create the file */
   if (!fp) {
      fp = fopen(file, "w+");
      if (!fp) {
         ERROR(MSG_HEART_CANNOTOPEN_SS, file, strerror(errno));
         DRETURN(-1);
      }
   }

   /* Read the heartbeat file which should contain only
    * one matching input item 
    */
   if (fscanf(fp, "%d", &hb) != 1) {
      /* can't read file, reset hb */
      hb = 1;
   } else {
      /* now increase hb */
      hb++;
      if (hb > 99999) {
         hb = 1;
      }   
   }

   /* seek to beginning of file */
   if ( fseek(fp, 0, 0) != 0 ) {
      ERROR(MSG_HEART_CANNOT_FSEEK_SS, file, strerror(errno));
      DRETURN(-2);   
   }

   /* write in curent data (always write 5 characters) */
   if (fprintf(fp, "%05d\n", hb) == EOF) {
      FCLOSE(fp);
      ERROR(MSG_HEART_WRITE_ERROR_SS, file, strerror(errno));
      DRETURN(-3);
   }
   FCLOSE(fp);
   
   /* This is only for testsuite testing */
   if (sge_testmode_timeout_value > 0 && hb == sge_testmode_timeout_at_heartbeat ) {
      sleep(sge_testmode_timeout_value);
   }


   gettimeofday(&end_time,nullptr);
   write_time = end_time.tv_sec - start_time.tv_sec;
   if (write_time > write_timeout) {
      WARNING(MSG_HEART_WRITE_TIMEOUT_S, file);
      return -4;
   }

   if (beat_value != nullptr) {
      *beat_value = hb;
   }

   DRETURN(0);
FCLOSE_ERROR:
   ERROR(MSG_HEART_CLOSE_ERROR_SS, file, strerror(errno));
   DRETURN(-5);
}

void set_inc_qmaster_heartbeat_test_mode(int value) {
   if (value > 0) {
      sge_testmode_timeout_value = value;  
      sge_testmode_timeout_at_heartbeat = 100;
   }
}
