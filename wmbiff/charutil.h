/* $Id: charutil.h,v 1.2 2001/06/19 03:38:58 dwonis Exp $ */
/* Author: Mark Hurley  (debian4tux@telocity.com)
 *
 * Simple char util's to trim char string arrays
 *
 */

#ifndef CHARUTIL
#define CHARUTIL

inline int LeftTrim(char *psValue);
inline int RightTrim(char *psValue);
inline int FullTrim(char *psValue);

#endif
