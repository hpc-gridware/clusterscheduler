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

#include <unistd.h>
#include <cerrno>
#include <cstring>

#include <termios.h>

static struct termios prev_termios;
static int g_raw_mode = 0;

/**
 * @brief Test whether we currently own the controlling terminal on STDOUT.
 *
 * Calling tcsetattr() on the controlling terminal from a background process
 * group raises SIGTTOU, which stops the whole client. Raw-mode enter/leave must
 * therefore only touch the terminal when we are its foreground process group.
 *
 * @return true if STDOUT's foreground process group is our process group
 */
static bool terminal_is_foreground() {
   return tcgetpgrp(STDOUT_FILENO) == getpgrp();
}

/**
 * @brief Set the terminal to raw mode.
 *
 * Puts STDOUT's controlling terminal into raw mode (control characters are no
 * longer interpreted, they are simply printed). When the client runs in the
 * background, applying the mode now would raise SIGTTOU and stop the client, so
 * the change is deferred: the function returns 0 without modifying the terminal
 * and the SIGCONT re-entry path re-applies it once we are in the foreground.
 *
 * @return 0 on success (or when deferred while backgrounded), else errno
 */
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

      if (!terminal_is_foreground()) {
         /* Backgrounded: applying now would SIGTTOU-stop us. Defer - the SIGCONT
          * re-entry path re-applies raw mode once we are foreground. */
         return 0;
      }
      if (tcsetattr(STDOUT_FILENO, TCSADRAIN, &tio) == -1) {
         ret = errno;
      } else {
         g_raw_mode = 1;
      }
   }
   return ret;
}

/**
 * @brief Restore the previous terminal mode.
 *
 * Undoes terminal_enter_raw_mode(). If the client is currently backgrounded the
 * restoring tcsetattr() would raise SIGTTOU (e.g. during teardown or atexit), so
 * it is skipped best-effort; the shell typically resets the tty on prompt return.
 *
 * @return 0 on success (or when skipped while backgrounded), else errno
 */
int terminal_leave_raw_mode() {
   int ret = 0;

   if (g_raw_mode == 1) {
      if (!terminal_is_foreground()) {
         /* best-effort: don't SIGTTOU during teardown/atexit while backgrounded */
         return 0;
      }
      if (tcsetattr(STDOUT_FILENO, TCSADRAIN, &prev_termios) == -1) {
         ret = errno;
      } else {
         g_raw_mode = 0;
      }
   }
   return ret;
}

