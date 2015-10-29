//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Eye-script parser class
//-----------------------------------------------------------------------------
//   This program is free software : you can redistribute it and / or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation, either version 3 of the License, or
//   (at your option) any later version.
//
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//   GNU General Public License for more details.
//
//   You should have received a copy of the GNU General Public License
//   along with this program. If not, see <http://www.gnu.org/licenses/>.
//-----------------------------------------------------------------------------
//   NOTE:  Any work derived from this code must contain a notice in the
//			user interface and any accompanying documentation stating that
//          is based on FMJ-Software's Image Eye and the ieC++ library!
//-----------------------------------------------------------------------------
//   If you'd like to release a closed-source product which uses this code,
//   commercial licenses are also available. For more information about such
//   licensing, please visit <http://www.fmjsoft.com/>
//-----------------------------------------------------------------------------

#include "ImageEye.h"
#pragma hdrstop

#include "EyeScriptParser.h"

//-----------------------------------------------------------------------------

EyeScriptParser::EyeScriptParser(PCBYTE _pbBeg, PCBYTE _pbEnd, int _nMaxTokenLen)
:	pbBeg(_pbBeg), pbEnd(_pbEnd), pbPos(_pbBeg), nMaxTokenLen(_nMaxTokenLen-1)
{
}


bool EyeScriptParser::FindNextToken()
{
	while (!EndOfScript()) {
    	BYTE c = *pbPos;
        switch (c) {
        // Skip deliminator characters
        case 0:
		case 9:
        case 10:
        case 13:
        case ' ':
		case '=':
        case ';':
        	break;
        // A comment?
        case '/':
			if (pbPos+1 >= pbEnd) return true;
        	switch (pbPos[1]) {
            case '/':	// Yes, a single-liner
	           	while ((++pbPos < pbEnd) && (*pbPos != 10) && (*pbPos != 13));
    	        break;
            case '*':	// Yes, a multi-liner
	           	while ((++pbPos < pbEnd) && ((pbPos[0] != '*') || (pbPos[1] != '/')));
                pbPos++;
    	        break;
            default:	// No, a token
            	return true;
            }
            break;
        // Ok, we have a new token!
        default:
        	return true;
        }
        pbPos++;
    }
    // End Of File
    return false;
}


bool EyeScriptParser::FindEndOfToken()
{
	int nLen = 0;
	while (!EndOfScript()) {
    	BYTE c = *pbPos;
        switch (c) {
        // A deliminator character?
        case 0:
		case 9:
        case 10:
        case 13:
        case ' ':
		case '=':
        	return true;
        // An end of command character?
        case ';':
        	return false;
        // A comment?
        case '/':
			if (pbPos+1 >= pbEnd) return false;
        	switch (pbPos[1]) {
            case '/':
            case '*':
            	return true;
            default:
            	break;
            }
            break;
        }
        pbPos++;
		if (++nLen > nMaxTokenLen) return false;
    }
    // End Of File
    return false;
}


bool EyeScriptParser::ParseToken(char *pszToken, bool *pbMoreFollows)
{
	if (!FindNextToken()) {
		*pszToken = 0;
		if (pbMoreFollows) *pbMoreFollows = false;
		return false;
	}

	PCBYTE pbToken = pbPos;
	bool bMore = FindEndOfToken();

	int iTokenLen;
    if ((pbToken[0] == '"') && (pbToken[1] != '"')) {
        pbToken++;
		while (!EndOfScript()) {
            if (pbPos[-1] == '"') break;
			if (!FindNextToken()) {
				bMore = false;
				break;
			}
            bMore = FindEndOfToken();
        }
        iTokenLen = int(pbPos-1 - pbToken);
    } else {
    	iTokenLen = int(pbPos - pbToken);
    }
	if ((iTokenLen > nMaxTokenLen) || (iTokenLen < 1)) return false;
    memcpy(pszToken, pbToken, iTokenLen);
   	pszToken[iTokenLen] = 0;

	if (pbMoreFollows) *pbMoreFollows = bMore;

	return true;
}


bool EyeScriptParser::ParseInt(int &i, bool *pbMoreFollows)
{
	char sz[256], *psz = sz;
    if (!ParseToken(sz, pbMoreFollows)) return false;

	bool bNegate = false;
	if (*psz == '+') {
		psz++;
	} else if (*psz == '-') {
		bNegate = true;
		psz++;
	}

	bool bHex = (toupper(psz[strlen(psz)-1]) == 'H');

	for (i = 0; ; ) {
		char c = *psz++;
		if (!c) break;
		if (bHex) {
			if ((c >= '0') && (c <= '9')) c = (c - '0');
			else if ((c >= 'a') && (c <= 'f')) c = (c+10 - 'a');
			else if ((c >= 'A') && (c <= 'F')) c = (c+10 - 'A');
			else break;
			i = i*16 + c;
		} else {
			if (!((c >= '0') && (c <= '9'))) break;
			i = i*10 + (c - '0');
		}
	}

	if (bNegate) i = -i;

    return true;
}


bool EyeScriptParser::ParseBool(bool &b, bool *pbMoreFollows)
{
	char sz[256];
    if (!ParseToken(sz, pbMoreFollows)) return false;
	b = (	(_strcmpi(sz, "On") == 0) || (_strcmpi(sz, "1") == 0) || 
			(_strcmpi(sz, "true") == 0) || (_strcmpi(sz, "yes") == 0)	);
    return true;
}
