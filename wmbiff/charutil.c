/* $Id: charutil.c,v 1.4 2001/10/04 09:50:59 jordi Exp $ */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
