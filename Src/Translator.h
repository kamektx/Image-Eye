//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Language translation
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

#pragma once

#ifndef _INC_TCHAR
#include <tchar.h>
#endif
typedef TCHAR *PTCHAR;
typedef const TCHAR *PCTCHAR;


class Translator {
public:
	Translator();
	~Translator();
	
	bool LoadDictionaryFromFile(PCTCHAR pcszFile);			// Load a translation file, compile it and select it as the current dictionary (wrapper for CompileFile2Dictionary + SelectDictionary)

	void *CompileFile2Dictionary(PCTCHAR pcszFile, DWORD &cbDictionarySize);	// Load a translation file and compilte it to a dictionary
	void SelectDictionary(void *pDictionary_) { pDictionary = pDictionary_; }	// Select active dictionary

	void FreeDictionary(void *pcDictionary) { delete[] pDictionary; }

	PCTCHAR Translate(PCTCHAR pcszText) const;				// Translate a text using the current dictionary (returns the original string itself of not in dictionary)
	PCTCHAR TranslateX(PCTCHAR pcszText) const;				// Like Translate() except that NULL is returned if it's not in dictionary
	void TranslateCtrls(HWND hwnd) const;					// Translate all the controls of a dialog box

protected:

	typedef struct {
		WORD wTotalTextLength;					// Total length of original and translated texts that follows, including NULL's, the next TPhrase is found after thus many TCHAR's
	} TPhrase;

	typedef struct {
		WORD wOffsFirstPhrase;					// Char-offset from start of dictionary to a series of consecutive TPhrase, terminated by a TPhrase::wTotalTextLength = 0. THashEntry::wOffsFirstPhrase == 0 iff there a no phrases for the hash
	} THashEntry;

	typedef THashEntry TDictionaryHash[128];	// Array of lists strings, use HashOfText() to index the array

	int HashOfText(PCTCHAR pcsz) const;

	void *pDictionary;
};

extern Translator g_Language;
