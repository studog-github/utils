/*
 * studog's xxd implementation
 */

#include <string.h>
#include <stdio.h>

#include "xxd.h"

static char printable(char c) {
   if (c < 0x20 || c > 0x7e)
      return '.';
   return c;
}

#define LINE_LEN   69
static char line[LINE_LEN];
#define ASCII_OFFSET   51

void xxd(unsigned char *buf, unsigned int len, int (*print)(const char *format, ...)) {
   unsigned int i;
   unsigned int j;
   char *xptr;
   char *cptr;

   memset(line, ' ', LINE_LEN - 2);
   line[LINE_LEN - 2] = '\n';
   line[LINE_LEN - 1] = '\0';

   i = 0;
   while (i < len) {
      xptr = line;
      xptr += sprintf(xptr, "%08x: ", i);
      cptr = line + ASCII_OFFSET;
      for (j = 0; j < 16 && i < len; j++, i++) {
         *cptr++ = printable(buf[i]);
         xptr += sprintf(xptr, "%02x", buf[i]);
         if (j % 2) {
            *xptr++ = ' ';
         }
      }
      if (j < 16) {
         memset(xptr, ' ', (line + ASCII_OFFSET - 2) - xptr);
         memset(cptr, ' ', 16 - j);
      }
      print("%s", line);
   }
}
