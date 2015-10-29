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

#include "ImageEye.h"
#pragma hdrstop

#include "ieTextConv.h"
#include "Translator.h"

Translator g_Language;


//-----------------------------------------------------------------------------

Translator::Translator()
:	pDictionary(nullptr)
{
}


Translator::~Translator()
{
}


bool Translator::LoadDictionaryFromFile(PCTCHAR pcszFile)
{
	DWORD cbDictionarySize;
	void *pDictionary = g_Language.CompileFile2Dictionary(pcszFile, cbDictionarySize);
	if (!pDictionary) return false;
	
	SelectDictionary(pDictionary);
	return true;
}


int Translator::HashOfText(PCTCHAR pcsz) const
{
	static const DWORD adwPrimes[8] = { 0, 1, 3, 5, 7, 11, 13, 17 };

	DWORD h = 0;

	for (int i = 0; i < 8; i++) {
		
		DWORD c = *pcsz++;
		if (c <= 1) break;

		c += adwPrimes[i];
#ifdef UNICODE
		c = c ^ (c >> 7) ^ (c >> 14);
#else
		c = c ^ (c >> 7);
#endif
		h ^= c;
	}

	return int(h & 0x7F);
}


static void ParseUntoMarker(PTCHAR pszDst, PTCHAR &pszSrc)
{
	// Parse source text, i.e. up until | Remaining text is the translated text
	for (;;) {

		TCHAR c = *pszSrc++;
		
		if (!c) break;			// End of string marker?

		if (c == '|') {			// Dictionary split marker?
			if (*pszSrc != '|') break;
			pszSrc++;

		} else if (c == '\\') {	// Escape character?
		
			c = *pszSrc++;

			if (c == 'n') {						// \n is used to encode ascii 10
				c = 10;

			} else if (c == 't') {
				c = 8;
			
			} else if ((c >= '0') && (c <= '9')) {

				int m = int(c - '0');					
				int cc = 3;
				while (--cc > 0) {
					c = *pszSrc;
					if ((c < '0') || (c > '9')) 
						break;
					m = m*10 + int(c - '0');
					pszSrc++;
				}
				c = TCHAR(m);
			}
		}

		*pszDst++ = c;
	}

	*pszDst = 0;
}


void *Translator::CompileFile2Dictionary(PCTCHAR pcszFile, DWORD &cbDictionarySize)
{
	cbDictionarySize = 0;

	// Read translation file into memory
	IE_HFILE hf = ief_Open(pcszFile);
	if (hf == IE_INVALIDHFILE) return nullptr;

	bool bReadOk = false;
	PBYTE pTransFile = nullptr;
	DWORD cbTransFile = ief_Size(hf);

	for (;;) {
		if (cbTransFile <= 2) break;

		pTransFile = new BYTE[cbTransFile + 2];
		if (!pTransFile) break;

		if (!ief_Read(hf, pTransFile, cbTransFile)) break;

		bReadOk = true;
		break;
	}

	ief_Close(hf);

	if (!bReadOk) {
		if (pTransFile) delete[] pTransFile;
		return nullptr;
	}

	// Check if its Unicode
	bool bByteSwap = (*PWORD(pTransFile) == 0xFFFE);
	bool bUnicode = (*PWORD(pTransFile) == 0xFEFF) || bByteSwap;

	// Convert to TCHAR format
	PTCHAR pcSrc, pcSrcEnd;
#ifdef UNICODE
	if (bUnicode) {
		pcSrc = (wchar_t *)(pTransFile + 2);
		pcSrcEnd = (wchar_t *)(pTransFile + (cbTransFile & ~1));
		if (bByteSwap) {
			for (wchar_t *pwc = pcSrc; pwc < pcSrcEnd; )
				*pwc++ = (wchar_t)SwapW(WORD(*pwc));
		}
	} else {
		pcSrc = new wchar_t[cbTransFile];
		if (!pcSrc) { delete[] pTransFile; return nullptr; }
		pcSrcEnd = pcSrc + cbTransFile;
		ie_Latin1ToUnicode(pcSrc, (char *)pTransFile, cbTransFile);
		delete[] pTransFile;
		pTransFile = (PBYTE)pcSrc;
	}
#else
	pcSrc = (char *)pTransFile;
	if (bUnicode) {
		wchar_t *pwcSrc = (wchar_t *)(pTransFile + 2), *pwc;
		int nChars = (cbTransFile-2) >> 1, n;
		pcSrcEnd = pcSrc + nChars;
		if (bByteSwap) {
			for (pwc = pwcSrc, n = nChars; n--; )
				*pwc++ = (wchar_t)SwapW(WORD(*pwc));
		}
		mdUnicodeToLatin1(pcSrc, pwcSrc, nChars);
	} else {
		pcSrcEnd = (char *)(pTransFile + cbTransFile);
	}
#endif

	// Convert into a list of phrases stored as original\0translation\0\...
	// and also calculate some statistics we'll need
	PTCHAR pcDst = (PTCHAR)pTransFile;
	TCHAR c;
	int iHash;

	struct {
		int nPhrases;
		int nChars;
		int nWriteOffs;
	} aHashStat[128];
	iem_Zero(aHashStat, sizeof(aHashStat));

	for (;;) {

		// Skip new line and line-feeds (i.e. skip until beginning of a non-empty line)
		while ((pcSrc < pcSrcEnd) && (*pcSrc == 10) || (*pcSrc == 13))
			pcSrc++;
		if (pcSrc >= pcSrcEnd) break;

		if ((pcSrc[0] == '/') && (pcSrc[1] == '/')) {
			// Skip comment line
			while ((pcSrc < pcSrcEnd) && (*pcSrc != 10) && (*pcSrc != 13))
				pcSrc++;
			continue;
		}

		// Copy until | divisor (i.e. copy original text)
		PTCHAR pcOriginalText = pcDst;

		while (pcSrc < pcSrcEnd) {
			c = *pcSrc++;
			if ((c == '|') || (c == 10) || (c == 13)) break;
			if (c == '\\') {
				if (*pcSrc == 'n') {
					*pcDst++ = 10;
					continue;
				} else if (*pcSrc == 't') {
					*pcDst++ = 8;
					continue;
				}
			}
			*pcDst++ = c;
		}

		if (c != '|') {
			// Syntax error: no divisor; backtrack, then go on with next line
			pcDst = pcOriginalText;
			continue;
		}

		*pcDst++ = 0;

		// Copy until new-line (i.e. copy translated text)
		PTCHAR pcTranslatedText = pcDst;

		while (pcSrc < pcSrcEnd) {
			c = *pcSrc++;
			if ((c == 10) || (c == 13)) break;
			if (c == '\\') {
				if (*pcSrc == 'n') {
					*pcDst++ = 10;
					continue;
				} else if (*pcSrc == 't') {
					*pcDst++ = 8;
					continue;
				}
			}
			*pcDst++ = c;
		}

		*pcDst++ = 0;

		// Sanity checks
		if (!*pcOriginalText || !*pcTranslatedText || !_tcscmp(pcOriginalText, pcTranslatedText)) {
			pcDst = pcOriginalText;
			continue;
		}

		iHash = HashOfText(pcOriginalText);
		aHashStat[iHash].nPhrases++;
		aHashStat[iHash].nChars += int(pcDst - pcOriginalText);
	}

	int nTotalPhrases = 0, nTotalChars = 0, nOccupiedHashEntries = 0;
	int nPhraseOffs = sizeof(TDictionaryHash) / sizeof(TCHAR);

	for (iHash = 0; iHash < 128; iHash++) {

		if (!aHashStat[iHash].nPhrases) continue;

		nOccupiedHashEntries++;

		nTotalPhrases += aHashStat[iHash].nPhrases;
		nTotalChars += aHashStat[iHash].nChars;

		aHashStat[iHash].nWriteOffs = nPhraseOffs;
		nPhraseOffs += (((aHashStat[iHash].nPhrases + 1)*sizeof(TPhrase))/sizeof(TCHAR)) + aHashStat[iHash].nChars;
	}

	pcSrc = (PTCHAR)pTransFile;
	pcSrcEnd = (PTCHAR)pcDst;


	// Allocate dictionary

	cbDictionarySize = sizeof(TDictionaryHash) + (nTotalPhrases+nOccupiedHashEntries)*sizeof(TPhrase) + nTotalChars*sizeof(TCHAR);

	PBYTE pDictionary = new BYTE[cbDictionarySize];
	if (!pDictionary) { delete[] pTransFile; cbDictionarySize = 0; return nullptr; }

	// Store phrases in dictionary
	PTCHAR pcDictBase = (PTCHAR)pDictionary;
	TDictionaryHash *pDH = (TDictionaryHash *)pcDictBase;

	for (iHash = 0; iHash < 128; iHash++) {
		(*pDH)[iHash].wOffsFirstPhrase = (WORD)aHashStat[iHash].nWriteOffs;
	}

	while (pcSrc < pcSrcEnd) {

		PTCHAR pszOriginalText = pcSrc;
		int nOriginalTextLen = int(_tcslen(pszOriginalText)) + 1;
		pcSrc += nOriginalTextLen;

		PTCHAR pszTranslatedText = pcSrc;
		int nTranslatedTextLen = int(_tcslen(pszTranslatedText)) + 1;
		pcSrc += nTranslatedTextLen;

		iHash = HashOfText(pszOriginalText);
		TPhrase *pPhrase = (TPhrase *)(pcDictBase + aHashStat[iHash].nWriteOffs);

		pPhrase->wTotalTextLength = (WORD)(nOriginalTextLen + nTranslatedTextLen);
		aHashStat[iHash].nWriteOffs += sizeof(TPhrase)/sizeof(TCHAR);

		iem_Copy(pcDictBase + aHashStat[iHash].nWriteOffs, pszOriginalText, nOriginalTextLen*sizeof(TCHAR));
		aHashStat[iHash].nWriteOffs += nOriginalTextLen;

		iem_Copy(pcDictBase + aHashStat[iHash].nWriteOffs, pszTranslatedText, nTranslatedTextLen*sizeof(TCHAR));
		aHashStat[iHash].nWriteOffs += nTranslatedTextLen;
	}

	for (iHash = 0; iHash < 128; iHash++) {
		if (aHashStat[iHash].nWriteOffs) {
			TPhrase *pPhrase = (TPhrase *)(pcDictBase + aHashStat[iHash].nWriteOffs);
			pPhrase->wTotalTextLength = 0;
		}
	}

	delete[] pTransFile;

	return pDictionary;
}


PCTCHAR Translator::Translate(PCTCHAR pcszText) const
{
	if (!pDictionary || !pcszText || !*pcszText) return pcszText;
	int nOriginalTextLength = 0;
	for (;;)
		if (pcszText[++nOriginalTextLength] <= 1)
			break;

	const THashEntry &HE = (*(const TDictionaryHash *)pDictionary)[HashOfText(pcszText)];
	if (!HE.wOffsFirstPhrase) return pcszText;

	const TPhrase *pPhrase = (const TPhrase *)(((PTCHAR)pDictionary) + HE.wOffsFirstPhrase);

	for (;;) {
		
		int nTotalTextLength = pPhrase->wTotalTextLength;
		if (!nTotalTextLength) return pcszText;

		PCTCHAR pcszOriginalText = PCTCHAR(pPhrase + 1);
		if ((nTotalTextLength > nOriginalTextLength) &&
			!pcszOriginalText[nOriginalTextLength] &&
			!memcmp(pcszText, pcszOriginalText, nOriginalTextLength*sizeof(TCHAR))) {
			
			PCTCHAR pcszTranslatedText = (pcszOriginalText + nOriginalTextLength + 1);
			return pcszTranslatedText;
		}

		pPhrase = (TPhrase *)(pcszOriginalText + nTotalTextLength);
	}
}


PCTCHAR Translator::TranslateX(PCTCHAR pcszText) const
{
	PCTCHAR pcszTrans = Translate(pcszText);
	return (pcszTrans != pcszText) ? pcszTrans : nullptr;
}


void Translator::TranslateCtrls(HWND hwnd) const
{
	if (!pDictionary) return;

	TCHAR sz[512];

	if (GetWindowText(hwnd, sz, sizeof(sz)/sizeof(TCHAR))) {
		if (*sz) {
			PCTCHAR pcsz = Translate(sz);
			if (*pcsz) {
				SetWindowText(hwnd, pcsz);
			}
		}
	}

	for (HWND hwndChild = FindWindowEx(hwnd, NULL, NULL, NULL); hwndChild; hwndChild = FindWindowEx(hwnd, hwndChild, NULL, NULL)) {
		TranslateCtrls(hwndChild);
	}
}
