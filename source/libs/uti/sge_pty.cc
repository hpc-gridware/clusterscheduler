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
 *  Portions of this software are Copyright (c) 2023-2025 HPC-Gridware GmbH
 *
 ************************************************************************/
/*___INFO__MARK_END__*/

#include <unistd.h>
#include <cerrno>
#include <cstring>

#include <termios.h>

static struct termios prev_termios;
static int g_raw_mode = 0;

/****** uti/pty/terminal_enter_raw_mode() **************************************
*  NAME
*     terminal_enter_raw_mode() -- Sets terminal to raw mode 
*
*  SYNOPSIS
*     int terminal_enter_raw_mode() 
*
*  FUNCTION
*     Sets terminal to raw mode, i.e. no control characters are interpreted any
*     more, but are simply printed.
*
*  RESULT
*     int - 0 if Ok, else errno
*
*  NOTES
*     MT-NOTE: terminal_enter_raw_mode() is not MT safe 
*
*  SEE ALSO
*     pty/terminal_leave_raw_mode
*******************************************************************************/
int terminal_enter_raw_mode() {
   struct termios tio {};
   int ret = 0;

   if (tcgetattr(STDOUT_FILENO, &tio) == -1) {
      ret = errno;
   } else {
      memcpy(&prev_termios, &tio, sizeof(struct termios));
      tio.c_iflag |= IGNPAR;
      tio.c_iflag &= ~(BRKINT | ISTRIP | INLCR | IGNCR | ICRNL | IXANY | IXOFF);
#ifdef IUCLC
      tio.c_iflag &= ~IUCLC;
#endif
      tio.c_lflag &= ~(ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHONL);
#ifdef IEXTEN
      tio.c_lflag &= ~IEXTEN;
#endif
      tio.c_oflag &= (OPOST | ONLCR);
      tio.c_cc[VMIN] = 1;
      tio.c_cc[VTIME] = 0;

      if (tcsetattr(STDOUT_FILENO, TCSADRAIN, &tio) == -1) {
         ret = errno;
      } else {
         g_raw_mode = 1;
      }
   }
   return ret;
}

/****** uti/pty/terminal_leave_raw_mode() **************************************
*  NAME
*     terminal_leave_raw_mode() -- restore previous terminal mode
*
*  SYNOPSIS
*     int terminal_leave_raw_mode() 
*
*  FUNCTION
*     Restores the previous terminal mode.
*
*  RESULT
*     int - 0 if Ok, else errno
*
*  NOTES
*     MT-NOTE: terminal_leave_raw_mode() is not MT safe 
*
*  SEE ALSO
*     pty/terminal_enter_raw_mode()
*******************************************************************************/
int terminal_leave_raw_mode() {
   int ret = 0;

   if (g_raw_mode == 1) {
      if (tcsetattr(STDOUT_FILENO, TCSADRAIN, &prev_termios) == -1) {
         ret = errno;
      } else {
         g_raw_mode = 0;
      }
   }
   return ret;
}

