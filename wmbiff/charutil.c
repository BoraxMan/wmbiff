/* $Id: charutil.c,v 1.3 2001/06/19 03:38:58 dwonis Exp $ */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "charutil.h"

inline int LeftTrim(char *psValue)
{

	char *psTmp = psValue;

	while (*psTmp == ' ' || *psTmp == '\t')
		psTmp++;

	strcpy(psValue, psTmp);

	return EXIT_SUCCESS;
}

inline int RightTrim(char *psValue)
{

	long lLength = strlen(psValue) - 1;

	while ((psValue[lLength] == ' ' || psValue[lLength] == '\t')
		   && *psValue) {
		lLength--;
	}

	psValue[++lLength] = '\000';
	return EXIT_SUCCESS;
}

inline int FullTrim(char *psValue)
{

	if (LeftTrim(psValue) != 0)
		return EXIT_FAILURE;
	if (RightTrim(psValue) != 0)
		return EXIT_FAILURE;
	return EXIT_SUCCESS;
}
