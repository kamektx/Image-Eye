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


class EyeScriptParser {
public:
	EyeScriptParser(PCBYTE pbBeg, PCBYTE pbEnd, int nMaxTokenLen);

	bool ParseToken(char *pszToken, bool *pbMoreFollows = nullptr);
	bool ParseInt(int &i, bool *pbMoreFollows = nullptr);
	bool ParseBool(bool &b, bool *pbMoreFollows = nullptr);
	
	DWORD GetParsePoint() { return DWORD(pbPos - pbBeg); }
	void SetParsePoint(DWORD dwPos) { pbPos = pbBeg+dwPos; }
	
	bool EndOfScript() { return pbPos >= pbEnd; }

protected:
	bool FindNextToken();
	bool FindEndOfToken();
	PCBYTE pbBeg, pbEnd, pbPos;
	int	nMaxTokenLen;
};
