/* $Id: charutil.h,v 1.4 2001/11/16 00:40:52 bluehal Exp $ */
/* Author: Mark Hurley  (debian4tux@telocity.com)
 *
 * Character / string manipulation utilities. 
 *
 */

#ifndef CHARUTIL
#define CHARUTIL

#include <regex.h>

int FullTrim(char *psValue);

void Bin2Hex(unsigned char *src, int length, char *dst);

void Encode_Base64(char *src, char *dst);
void Decode_Base64(char *src, char *dst);

/* helper function for the configuration line parser */
void copy_substring(char *destination,
					int startidx, int endidx, const char *source);

/* common to Pop3 and Imap4 authentication list grabber. */
void grab_authList(const char *source, char *destination);

/* handles main regex work */
int compile_and_match_regex(const char *regex, const char *str,
							/*@out@ */ struct re_registers *regs);
#endif
