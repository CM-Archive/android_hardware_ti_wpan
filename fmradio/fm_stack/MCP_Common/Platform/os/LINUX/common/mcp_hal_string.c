/*
 * TI's FM Stack
 *
 * Copyright 2001-2010 Texas Instruments, Inc. - http://www.ti.com/
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*******************************************************************************\
*
*   FILE NAME:      mcp_hal_string.c
*
*   DESCRIPTION:    This file implements the MCP HAL string utilities for LINUX.
*
*   AUTHOR:         Chen Ganir
*
\*******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
/* #include "osapi.h" TODO (a0798989): remove */
#include "mcp_hal_string.h"

/****************************************************************************
 *
 * Local Prototypes
 *
 ***************************************************************************/

/****************************************************************************
 *
 * Local Functions Definitions
 *
 ***************************************************************************/
 static void itoa(int n, unsigned char s[]);
static void reverse(char s[]);
 
/****************************************************************************
 *
 * Public Functions
 *
 ***************************************************************************/

McpU8* MCP_HAL_STRING_strtok(const char *Str1, const char *Str2)
{
	return strtok(Str1,Str2);
}

McpU8* MCP_HAL_STRING_strupr(const char *Str1)
{

	char* ch = Str1;
	for(; *ch != '\n'; ch++)
	{
		if(*ch>='a' && *ch <='z')
			*ch -= 32;
	}
	return Str1;
}



McpU8 MCP_HAL_STRING_StrCmp(const char *Str1, const char *Str2)
{
	while (*Str1 == *Str2) {
        if (*Str1 == 0 || *Str2 == 0) {
            break;
        }
        Str1++;
        Str2++;
    }

    /* Return zero on success, just like the ANSI strcmp() */
    if (*Str1 == *Str2)
        return 0;

    return 1;
}

McpU8 MCP_HAL_STRING_StriCmp(const char *Str1, const char *Str2)
{	
	const McpU32 lowerToUpperDiff = 'a' - 'A';
	
	while (*Str1 != 0 || *Str2 != 0)
	{
		if ( 	(*Str1==*Str2) || 
			((*Str1 >= 'A') && (*Str1 <= 'Z') && ((char)(*Str1 + lowerToUpperDiff) == *Str2)) ||
			((*Str2 >= 'A') && (*Str2 <= 'Z') && ((char)(*Str2 + lowerToUpperDiff) == *Str1)))
		{
			Str1++;
			Str2++;
		}
		else
			return 1;
	}
	if (*Str1 == *Str2) /* both pointers reached NULL */
	        return 0;
	return 1;
}

McpU16 MCP_HAL_STRING_StrLen(const char *Str)
{
	const char  *cp = Str;

	while (*cp != 0) cp++;

	return (McpU16)(cp - Str);
}

char* MCP_HAL_STRING_StrCpy(char* StrDest, const char *StrSource)
{
	return strcpy(StrDest, StrSource);
}

char* MCP_HAL_STRING_StrnCpy(char* StrDest, const char *StrSource, McpU32 Count)
{
	return strncpy(StrDest, StrSource, Count);
}

char *MCP_HAL_STRING_StrCat(char *strDest, const char *strSource)
{
	return strcat(strDest, strSource);
}

char *MCP_HAL_STRING_StrrChr(const char *Str, McpS32 c)
{
	return strrchr(Str, c);
}

char *MCP_HAL_STRING_Strstr(const char *Str1, const char *Str2)
{
    return strstr(Str1,Str2);
}

McpS32 MCP_HAL_STRING_Sprintf(char* StrDest,const char* format,...)
{
	va_list ap;
	McpS32 len = 0;
	if (format != NULL){
		va_start(ap, format);
		len = vsprintf(StrDest, format, ap);
		va_end(ap);
	}
	return len;
}

void MCP_HAL_STRING_ItoA(McpU16 Num,McpU8* Buff)
{
	itoa(Num, Buff);
}

McpS16 MCP_HAL_STRING_AtoI(McpU8* Buff)
{
	if (Buff)
		return atoi(Buff);
	else
		return 0;
}

/******************************************************
*				Internal Functions 					          *
******************************************************/
static void itoa(int n, unsigned char s[])
{
    int i, sign;

    if ((sign = n) < 0)  /* record sign */
        n = -n;          /* make n positive */
    i = 0;
    do {       /* generate digits in reverse order */
        s[i++] = n % 10 + '0';   /* get next digit */
    } while ((n /= 10) > 0);     /* delete it */
    if (sign < 0)
        s[i++] = '-';
    s[i] = '\0';
    reverse(s);
} 

static void reverse(char s[])
{
    int i, j;
    char c;

    for (i = 0, j = strlen(s)-1; i<j; i++, j--) {
        c = s[i];
        s[i] = s[j];
        s[j] = c;
    }
}