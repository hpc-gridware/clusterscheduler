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

/** @file
 * @brief Buffered file reading and writing helpers
 */

#include <cstdio>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <cerrno>

#include "uti/sge_io.h"
#include "uti/sge_log.h"
#include "uti/sge_rmon_macros.h"
#include "uti/sge_stdio.h"
#include "uti/sge_stdlib.h"
#include "uti/sge_unistd.h"

#include "uti/msg_utilib.h"

/// size of the read buffer used while copying
#define BUFFER     4096
/// growth step when reading a file of unknown size
#define FILE_CHUNK (100*1024)

/**
 * @brief Read n bytes from file descriptor
 *
 * Read n bytes from file descriptor.
 *
 * @param sfd file descriptor
 * @param ptr pointer to buffer
 * @param n number of bytes
 *
 * @return number of bytes read
 *
 * @note MT-NOTE: sge_readnbytes() is MT safe
 *
 * @see #sge_writenbytes
 */
int sge_readnbytes(int sfd, char *ptr, int n) {
   int i;                       /* number of bytes read */
   int nleft = n;               /* number of bytes still to read */

   DENTER(BASIS_LAYER);
   DPRINTF("TOTAL BYTES TO BE READ %d\n", n);

   /* Read n bytes */
   while (nleft > 0) {
      i = read(sfd, ptr, nleft);
      DPRINTF("read %d bytes on fd %d\n", i, sfd);

      if (i < 0) {
         DPRINTF("sge_readnbytes: returning %d\n", i);
         DRETURN((i));
      } else {
         if (i == 0)
            break;
      }
      nleft -= i;
      ptr += i;
   }

   DPRINTF("sge_readnbytes: returning %d\n", nleft);
   DRETURN((n - nleft));

}

/**
 * @brief Write n bytes to file descriptor
 *
 * Write n bytes to file descriptor
 *
 * @param sfd file descriptor
 * @param ptr pointer to buffer
 * @param n number of bytes
 *
 * @return number of bytes written
 *
 * @note MT-NOTE: sge_writenbytes() is MT safe
 *
 * @see #sge_readnbytes
 */
int sge_writenbytes(int sfd, const char *ptr, int n) {
   DENTER(BASIS_LAYER);

   int i;                       /* number of bytes written */
   int nleft = n;               /* number of bytes still to write */

   /* Write n bytes */
   while (nleft > 0) {
      DTRACE;
      i = write(sfd, ptr, nleft);
      if (i == -1) {
         DPRINTF("write failed with error %d: %s\n", i, strerror(errno));
      } else {
         DPRINTF("wrote %d bytes on fd %d\n", i, sfd);
      }

      if (i <= 0) {
         DPRINTF("sge_writenbytes: returning %d\n", i);
         DRETURN((i));
      }
      nleft -= i;
      ptr += i;
   }

   DRETURN((n));
}

/**
 * @brief Compare two files
 *
 * Compare two files. They are equal if:
 *    - both of them have the same name
 *    - if a stat() succeeds for both files and
 *      i-node/device-id are equal
 *
 * we are not sure
 * - if stat() failes for at least one of the files
 *   (It could be that both pathes direct to the same
 *   file not existing)
 *
 * @param name0 1st filename
 * @param name1 2nd filename
 *
 * @return Identical? 0 - Yes. 1 - No they are not equivalent.
 *
 * @note MT-NOTE: sge_filecmp() is MT safe
 */
int sge_filecmp(const char *name0, const char *name1) {
   SGE_STRUCT_STAT buf0{}, buf1{};

   DENTER(TOP_LAYER);

   if (!strcmp(name0, name1)) {
      DRETURN(0);
   }

   if (SGE_STAT(name0, &buf0) < 0) {
      DRETURN(1);
   }

   if (SGE_STAT(name1, &buf1) < 0) {
      DRETURN(1);
   }

   if (buf0.st_ino == buf1.st_ino && buf0.st_dev == buf1.st_dev) {
      DRETURN(0);
   } else {
      DRETURN(1);
   }
}

/**
 * @brief Copy/append one file to another
 *
 * Copy/append content from 'src' to 'dst'
 *
 * @param src source filename
 * @param dst destination filename
 * @param mode mode
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: sge_copy_append() is MT safe
 */
int sge_copy_append(const char *src, const char *dst, sge_mode_t mode) {
/// @cond   function local copy buffer size
#define CPBUF 1024
/// @endcond

   char buf[CPBUF];
   int fdsrc, fddst, modus, rs, ws;
   bool error;

   DENTER(TOP_LAYER);

   if (src == nullptr || dst == nullptr || strlen(src) == 0 || strlen(dst) == 0 ||
       !(mode == SGE_MODE_APPEND || mode == SGE_MODE_COPY)) {
      DRETURN(-1);
   }
   if (!strcmp(src, dst)) {
      DRETURN(-1);
   }

   /* Return if source file doesn't exist */
   if ((fdsrc = SGE_OPEN2(src, O_RDONLY)) == -1) {
      DRETURN(-1);
   }

   if (mode == SGE_MODE_APPEND)
      modus = O_WRONLY | O_APPEND | O_CREAT;
   else
      modus = O_WRONLY | O_CREAT;

   if ((fddst = SGE_OPEN3(dst, modus, 0666)) == -1) {
      DRETURN(-1);
   }

   error = false;
   while (!error) {
      rs = read(fdsrc, buf, 512);
      if (rs == -1 && errno == EINTR)
         continue;
      else if (rs == -1)
         error = true;

      if (!error && rs > 0) {
         while (!error) {
            ws = write(fddst, buf, rs);
            if (ws == -1 && errno == EINTR)
               continue;
            else if (ws == -1) {
               error = true;
               break;
            } else
               break;
         }
      }
      if (error)
         break;
      if (rs == 0)
         break;
   }

   close(fdsrc);
   close(fddst);

   DRETURN((error ? -1 : 0));
}

/**
 * @brief Put binary stream into a string
 *
 * Read a binary steam from given file descriptor 'fp' and
 * write it into (dynamically) malloced buffer as "ASCII" format.
 *
 * "ASCII" format means:
 *       '\0' is written as '\\' '\0'
 *       '\\' is written as '\\' '\\'
 *       End of buffer is written as '\0'
 *
 * @param fp file descriptor
 * @param size size of the buffer used within this function
 *
 * @return malloced buffer
 *
 * @note MT-NOTE: sge_bin2string() is MT safe
 *
 * @see #sge_string2bin
 */
char *sge_bin2string(FILE *fp, int size) {
   int i, fd;
   char inbuf[BUFFER], outbuf[2 * BUFFER];
   char *inp, *outp;
   char *dstbuf;
   int len,             /* length of current tmp buffer */
   dstbuflen,       /* total length of destination buffer */
   chunksize,       /* chunks for realloc */
   lastpos,         /* last position in destination buffer */
   error;

   if ((fd = fileno(fp)) == -1)
      return nullptr;

   chunksize = 20480;

   if (size <= 0)       /* no idea about buffer, malloc in chunks */
      size = chunksize;

   dstbuf = sge_malloc(size + 1);
   SGE_ASSERT(dstbuf != nullptr);
   dstbuflen = size;
   lastpos = 0;

   error = false;

   while (!error) {
      i = read(fd, inbuf, BUFFER);
      if (i > 0) {
         inp = inbuf;
         outp = outbuf;
         while (inp < &inbuf[i]) {
            if (*inp == '\\') {
               *outp++ = '\\';
               *outp++ = '\\';
            } else if (*inp == '\0') {
               *outp++ = '\\';
               *outp++ = '0';
            } else
               *outp++ = *inp;
            inp++;
         }


         len = outp - outbuf;

         if (lastpos + len > dstbuflen) {
            if ((dstbuf = (char *)sge_realloc(dstbuf, lastpos + len + chunksize, 0)) == nullptr) {
               error = true;
               break;
            }
            dstbuflen = lastpos + len + chunksize;

         }

         memcpy(&dstbuf[lastpos], outbuf, len);
         lastpos += len;

      } else if (i == 0) {
         break;
      } else {
         if (errno != EINTR) {
            error = true;
            break;
         }
      }
   }

   if (error) {
      sge_free(&dstbuf);
      return nullptr;
   } else {
      if ((dstbuf = (char *)sge_realloc(dstbuf, lastpos + 1, 0)) == nullptr) {
         return nullptr;
      }
      dstbuf[lastpos] = '\0';
      return dstbuf;
   }
}

/**
 * @brief Write 'binary' string into file
 *
 * Write 'binary' string into file
 *
 * @param fp file descriptor
 * @param buf "ASCII" string (see sge_bin2string())
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: sge_string2bin() is MT safe
 *
 * @see #sge_bin2string
 */
int sge_string2bin(FILE *fp, const char *buf) {
   char outbuf[BUFFER];
   char *outp;
   int fd;

   if ((fd = fileno(fp)) == -1)
      return -1;

   if (!buf)
      return -1;

   while (*buf) {
      outp = outbuf;
      while (*buf && (outp - outbuf < BUFFER)) {
         if (*buf == '\\') {
            if (*(buf + 1) == '\\')
               *outp++ = '\\';
            else
               *outp++ = '\0';
            buf += 2;
         } else
            *outp++ = *buf++;
      }

      if (write(fd, outbuf, outp - outbuf) != outp - outbuf)
         return -1;
   }
   return 0;
}

/**
 * @brief Load file into string
 *
 * Load file into string. Returns a pointer to a string buffer containing
 * the file contents and the size of the buffer (= number of bytes read)
 * in the variable len.
 * If the file cannot be read (doesn't exist, permissions etc.), nullptr is
 * returned as buffer and len is set to 0.
 *
 * @param fname filename
 * @param len number of bytes read
 *
 * @return malloced string buffer
 *
 * @note MT-NOTE: sge_file2string() is MT safe
 *
 * @see #sge_string2file, #sge_stream2string
 */
char *sge_file2string(const char *fname, int *len) {
   FILE *fp;
   SGE_STRUCT_STAT statbuf;
   int size, i;
   char *str;

   DENTER(CULL_LAYER);

   /* initialize len - in case of errors we want to return 0 
    * JG: TODO: it would be better to return -1. Check if calling
    * functions would handle this situation.
    */
   if (len != nullptr) {
      *len = 0;
   }

   /* try file access, read file info */
   if (SGE_STAT(fname, &statbuf)) {
      DRETURN(nullptr);
   }

   size = statbuf.st_size;

   if ((fp = fopen(fname, "r")) == nullptr) {
      ERROR(MSG_FILE_FOPENFAILED_SS, fname, strerror(errno));
      DRETURN(nullptr);
   }

   if ((str = sge_malloc(size + 1)) == nullptr) {
      FCLOSE(fp);
      DRETURN(nullptr);
   }

   str[0] = '\0';

   /*
   ** With fread(..., size, 1, ...),
   ** Windows cannot read <size> bytes here, because in
   ** text mode the trailing ^Z is ignored.
   ** CRLF -> LF conversion reduces size even further.
   ** Therefore, the file has less than size-1 bytes if read
   ** in text (ascii) mode.
   ** Correctly, fread returns 0, because 0 elements of
   ** size <size> were read.
   */
   if (size > 0) {
      i = fread(str, size, 1, fp);
      if (i != 1) {
         ERROR(MSG_FILE_FREADFAILED_SS, fname, strerror(errno));
         sge_free(&str);
         FCLOSE(fp);
         DRETURN(nullptr);
      }
      str[size] = '\0';    /* delimit this string */
      if (len != nullptr) {
         *len = size;
      }
   }

   FCLOSE(fp);

   DRETURN(str);
   FCLOSE_ERROR:
DRETURN(nullptr);
}

/**
 * @brief Read string from stream
 *
 * Read string from stream
 *
 * @param fp file descriptor
 * @param len number of bytes read
 *
 * @return pointer to malloced string buffer
 *
 * @note MT-NOTE: sge_stream2string() is MT safe
 *
 * @see #sge_file2string, #sge_string2file
 */
char *sge_stream2string(FILE *fp, int *len) {
   char *str;
   int filled = 0;
   int malloced_len, i;

   DENTER(TOP_LAYER);

   if (!(str = sge_malloc(FILE_CHUNK))) {
      DRETURN(nullptr);
   }
   malloced_len = FILE_CHUNK;

   /* malloced_len-filled-1 cause we reserve space for \0 termination */
   while ((i = fread(&str[filled], 1, malloced_len - filled - 1, fp)) > 0) {
      filled += i;
      if (malloced_len == filled + 1) {
         str = (char *)sge_realloc(str, malloced_len + FILE_CHUNK, 0);
         if (str == nullptr) {
            DRETURN(nullptr);
         }
         malloced_len += FILE_CHUNK;
      }

      if (feof(fp)) {
         DPRINTF("got EOF\n");
         break;
      }
   }
   str[filled] = '\0';  /* nullptr termination */
   *len = filled;

   DRETURN(str);
}

/**
 * @brief Write string into file
 *
 * Write string into file
 *
 * @param str pointer to buffer
 * @param len number of bytes which should be written
 * @param fname filename
 *
 * @return error state 0 - OK -1 - Error
 *
 * @note MT-NOTE: sge_string2file() is MT safe
 *
 * @see #sge_file2string, #sge_stream2string
 */

/* #define USE_FOPEN */

int sge_string2file(const char *str, int len, const char *fname) {
#ifdef USE_FOPEN
   FILE *fp;
#else
   int fp = -1;
#endif

   DENTER(TOP_LAYER);

#ifdef USE_FOPEN
   if (!(fp = fopen(fname, "w")))
#else
   if (!(fp = open(fname, O_WRONLY | O_CREAT, 0666)))
#endif
   {
      ERROR(MSG_FILE_OPENFAILED_S, fname);
      DRETURN(-1);
   }
   if (!len) {
      len = strlen(str);
   }

#ifdef USE_FOPEN
   if (fwrite(str, len, 1, fp) != 1)
#else
   if (write(fp, str, len) != len)
#endif
   {
      int old_errno = errno;
      ERROR(MSG_FILE_WRITEBYTESFAILED_IS, len, fname);
#ifdef USE_FOPEN
      FCLOSE(fp);
#else
      if (close(fp) != 0) {
         goto FCLOSE_ERROR;
      }
#endif
      unlink(fname);
      errno = old_errno;
      DRETURN(-1);
   }

#ifdef USE_FOPEN
   FCLOSE(fp);
#else
   if (close(fp) != 0) {
      goto FCLOSE_ERROR;
   }
#endif
   DRETURN(0);
   FCLOSE_ERROR:
   ERROR(MSG_FILE_FCLOSEFAILED_SS, fname, strerror(errno));
   DRETURN(-1);
}          

