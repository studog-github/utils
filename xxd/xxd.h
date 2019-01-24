#ifndef __XXD_H__
#define __XXD_H__

/*
 * studog's xxd implementation
 */

void xxd(unsigned char *buf, unsigned int len, int (*print)(const char *format, ...));

#endif
