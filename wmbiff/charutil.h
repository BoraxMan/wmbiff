/* $Id: charutil.h,v 1.3 2001/10/04 09:50:59 jordi Exp $ */
/* Author: Mark Hurley  (debian4tux@telocity.com)
 *
 * Simple char util's to trim char string arrays
 *
 */

#ifndef CHARUTIL
#define CHARUTIL

int FullTrim(char *psValue);

void Bin2Hex(unsigned char *src, int length, char *dst);

void Encode_Base64(char *src, char *dst);
void Decode_Base64(char *src, char *dst);

#endif
