/* $Id: charutil.c,v 1.13 2002/07/04 01:07:28 bluehal Exp $ */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#ifdef USE_DMALLOC
#include <dmalloc.h>
#endif
#include "charutil.h"

static __inline__ int LeftTrim(char *psValue)
{

	char *psTmp = psValue;

	while (*psTmp == ' ' || *psTmp == '\t')
		psTmp++;

	strcpy(psValue, psTmp);

	return EXIT_SUCCESS;
}

static __inline__ int RightTrim(char *psValue)
{

	long lLength = strlen(psValue) - 1;

	while ((psValue[lLength] == ' ' || psValue[lLength] == '\t')
		   && *psValue) {
		lLength--;
	}

	psValue[++lLength] = '\000';
	return EXIT_SUCCESS;
}

int FullTrim(char *psValue)
{

	if (LeftTrim(psValue) != 0)
		return EXIT_FAILURE;
	if (RightTrim(psValue) != 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}

void Bin2Hex(unsigned char *src, int length, char *dst)
{
	static char hex_tbl[] = "0123456789abcdef";

	int i = 0;
	char *ptr = dst;

	if (src && ptr) {
		for (i = 0; i < length; i++) {
			*ptr++ = hex_tbl[*src >> 4];
			*ptr++ = hex_tbl[*src++ & 0xf];
		}
		*ptr = 0;
	}
}


#define PAD '='
char ALPHABET[65] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/\000";

/* find char in in alphabet, return offset, -1 otherwise */
int find_char(char c)
{
	char *a = ALPHABET;

	while (*a)
		if (*(a++) == c)
			return a - ALPHABET - 1;

	return -1;
}

void Encode_Base64(char *src, char *dst)
{
	int g = 0;
	int c = 0;

	if (!src || !dst)
		return;

	while (*src) {
		g = (g << 8) | *src++;
		if (c == 2) {
			*dst++ = ALPHABET[0x3f & (g >> 18)];
			*dst++ = ALPHABET[0x3f & (g >> 12)];
			*dst++ = ALPHABET[0x3f & (g >> 6)];
			*dst++ = ALPHABET[0x3f & g];
		}
		c = (c + 1) % 3;
	}

	if (c) {
		if (c == 1) {
			*dst++ = ALPHABET[0x3f & (g >> 2)];
			*dst++ = ALPHABET[0x3f & (g << 4)];
			*dst++ = PAD;
			*dst++ = PAD;
		} else {
			*dst++ = ALPHABET[0x3f & (g >> 10)];
			*dst++ = ALPHABET[0x3f & (g >> 4)];
			*dst++ = ALPHABET[0x3f & (g << 2)];
			*dst++ = PAD;
		}
	}
	*dst = 0;
}


void Decode_Base64(char *src, char *dst)
{
	int g = 0;
	int c = 0;
	int n = 0;

	if (!src || !dst)
		return;

	while (*src) {
		n = find_char(*src++);
		if (n < 0)
			continue;

		g <<= 6;
		g |= n;
		if (c == 3) {
			*dst++ = g >> 16;
			*dst++ = g >> 8;
			*dst++ = g;
			g = 0;
		}
		c = (c + 1) % 4;
	}
	if (c) {
		if (c == 1) {
			/* shouldn't happen, but do something anyway */
			*dst++ = g << 2;
		} else if (c == 2) {
			*dst++ = g >> 4;
		} else {
			*dst++ = g >> 10;
			*dst++ = g >> 2;
		}
	}
	*dst = 0;
}

/* helper function for the configuration line parser */
void copy_substring(char *destination,
					int startidx, int endidx, const char *source)
{
	if (startidx > -1) {
		strncpy(destination, source + startidx, endidx - startidx);
		destination[endidx - startidx] = '\0';
	}
}

/* common to Pop3 and Imap4 authentication list grabber. */
void grab_authList(const char *source, char *destination)
{
	int i;
	/* regex isn't all that helpful for lists of things. */
	/* but does leave the end of the matched region in  regs.end[0] */
	/* what remains is a list of legal authentication schemes. */
	if (isalnum(source[0])) {
		/* copy, while turning caps into lower case */
		for (i = 0; i < 99 && source[i] != '\0'; i++) {
			destination[i] = tolower(source[i]);
		}
		destination[i] = '\0';
	} else {
		destination[0] = '\0';
	}
}


int compile_and_match_regex(const char *regex, const char *str,	/*@out@ */
							struct re_registers *regs)
{

	const char *errstr;
	int matchedchars;
	struct re_pattern_buffer rpbuf;

	/* compile the regex pattern */
	memset(&rpbuf, 0, sizeof(struct re_pattern_buffer));

	/* posix egrep interprets intervals (eg. {1,32}) nicely */
	re_syntax_options = RE_SYNTAX_POSIX_EGREP;
	errstr = re_compile_pattern(regex, strlen(regex), &rpbuf);
	if (errstr != NULL) {
		fprintf(stderr, "error in compiling regular expression: %s\n",
				errstr);
		return -1;
	}

	/* match the regex */
	regs->num_regs = REGS_UNALLOCATED;
	matchedchars = re_match(&rpbuf, str, strlen(str), 0, regs);
	/* this can fail (return -1 or 0) without being an error,
	   if we're trying to apply a regex just to see if it
	   matched. */

#ifdef undef
	printf("--\n");
	for (i = 1; i < 6; i++) {
		printf("%d %d, (%.*s)\n", regs.start[i], regs.end[i],
			   (regs.end[i] - regs.start[i]),
			   (regs.start[i] >= 0) ? &str[regs.start[i]] : "");
	}
#endif

	regfree(&rpbuf);			// added 3 jul 02, appeasing valgrind
	return matchedchars;
}

/* like perl chomp(); useful for dealing with input from popen */
void chomp(char *s)
{
	int l = strlen(s) - 1;
	if (l >= 0 && s[l] == '\n')
		s[l] = '\0';
}

char *strdup_ordie(const char *c)
{
	char *ret = strdup(c);
	if (ret == NULL) {
		fprintf(stderr, "ran out of memory\n");
		exit(EXIT_FAILURE);
	}
	return (ret);
}
