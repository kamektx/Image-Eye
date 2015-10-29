//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	An on-disk cache of precomputed icon images
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

#include "ieColor.h"
#include "ieTextConv.h"
#include "FileIcon.h"

extern void GIFLZW_Decompress(iePILUT pDst, DWORD cbMaxDst, PCBYTE pbSrc, DWORD cbSrc);							// in ief_gif.cpp
extern DWORD GIFLZW_Compress(PBYTE pbDst, iePCILUT pSrc, DWORD nX, DWORD nY, DWORD nBPL, int iBitsPerByte);		// in ief_gif.cpp
void DeltaDePredict(PBYTE pbData, DWORD dwSrcLen);	// forward declare
void DeltaPredict(PBYTE pbData, DWORD dwSrcLen);	// forward declare


//------------------------------------------------------------------------------------------
// .iei - Image Eye icon cache file format definition
//------------------------------------------------------------------------------------------

#define idRIFF		C2FOURC('R','I','F','F')
#define idIEIC		C2FOURC('I','E','I','C')	// IEI Cache

#define idIFMT		C2FOURC('I','F','M','T')	// Icon format
#define idFREF		C2FOURC('F','R','E','F')	// File reference

struct CHUNK {
	FOURC	fcId;
	DWORD	dwSize;
};

struct LIST {
	CHUNK	ck;
	FOURC	fcType;
};

struct IFMT {
	DWORD	NumFREFs		: 22;	// Number of FREC's in the FREC chunk
	DWORD	Reserved0		: 10;	// Reserved for future flags, set to 0
	DWORD	SizeOfFREF		: 8; 	// Size of an FREF struct excl. file name part, used for forward compatibility
	DWORD	IconXPix		: 10;	// Width in pixels of icon buttons
	DWORD	IconYPix		: 10;	// Height in pixels of icon buttons
	DWORD	DataCompression : 4; 	// Compression algorithm used in DATA
	#define fUncompressed	0
	#define fDeltaGIFLZW  	2
	DWORD	DataUncompSize	: 30; 	// Size of uncompressed data in DATA
	DWORD	FileNamesAreUTF8 : 1;	// Are file names encoded as UTF8? (otherwise they are in Windows 1252/Latin-1)
	DWORD	MetaDataIncluded : 1;	// Are the icon data followed by a meta data list? (Image Eye 8.0 and later)
};

struct FREF {
	DWORD	DataOffset		: 24;	// Icon image offset for uncompressed data (unpacked DATA chunk)
	DWORD	AlphaType		: 2; 	// Alpha type
	DWORD	DataOffsetHi	: 6; 	// Bit 24..30 of data offset (Image Eye v3.4, compiles from 2007-07-11 or later only)
	DWORD	IconXPix		: 10;	// Width in pixels of icon image
	DWORD	IconYPix		: 10;	// Height in pixels of icon image
	DWORD	IconFormat		: 4;	// Icon data format
	#define fBGR		0			// Raw BGR byte triplets (3 bytes per pixel)
	#define fBGRA		1			// Raw BGRA byte quads (4 bytes per pixel)
	DWORD	ImageZ			: 8;	// Color depth of image in the file
	WORD	ImageX; 				// Width of image in the file
	WORD	ImageY; 			 	// Height of image in the file
	DWORD	FileSize;				// File size stamp
	QWORD	FileTime;				// File time stamp
	//BYTE	szFileName[];			// File name, 0 terminated, encoded either as Latin-1 or as UTF-8 depending on IFMT::UseUTF8
};

#define idDATA		C2FOURC('D','A','T','A')	// File icon data
// ImageX*ImageY*bpp Bytes per image, at offsets given by FREF::DataOffset+FREF::DataOffsetHi, bpp=3 of 4 depending on IconFormat
// if IFMT::MetaDataIncluded then this is followed by a zero-terminated list of meta data, first a type BYTE (0=EndOfList,1=Comment), then a zero-terminated text in UTF8 format

//------------------------------------------------------------------------------------------
// End of .iei defitions
//------------------------------------------------------------------------------------------


//------------------------------------------------------------------------------------------

FileIconCache::FileIconCache()
	: strCacheFile(_T("ImageEye.iei")), pDATA(nullptr), pFREF(nullptr), iNumCacheRefs(0), iSizeOfFREF(0),
	bHaveCache(false), bUpdateCache(false), bFileNamesAreUTF8(false), bMetaDataIncluded(false)
{
}


FileIconCache::~FileIconCache()
{
	Free();
}


bool FileIconCache::RetrieveIcon(FileIcon *pIcon)
{
    if (!bHaveCache) return false;

#ifdef UNICODE
	char szName[3*MAX_PATH];
	if (bFileNamesAreUTF8) {
		ie_UnicodeToUTF8(szName, pIcon->GetFileNameStr(), sizeof(szName));
	} else {
		if (!ie_IsUnicode2Latin1Lossless(pIcon->GetFileNameStr()))
			*szName = 0;
		else
			ie_UnicodeToLatin1(szName, pIcon->GetFileNameStr(), sizeof(szName));
	}
	int nNameLen = strlen(szName);
#else
	int nNameLen = strlen(pIcon->GetFileNameStr());
#endif
	if (!nNameLen) return false;

	DWORD dwFileSizeLo = DWORD(pIcon->GetFileSize());
	IE_FILETIME ftFileTime = pIcon->GetFileTime();

    PBYTE p = pFREF;

	for (int i = iNumCacheRefs; i--; ) {

    	FREF *pfr = (FREF *)p;
        p += iSizeOfFREF;

        const char *pcszCacheName = (const char *)p;
		int nCacheNameLen = strlen(pcszCacheName);
        p += nCacheNameLen + 1;

		if (nCacheNameLen != nNameLen) continue;
		if (pfr->FileSize != dwFileSizeLo) continue;
        if (pfr->FileTime != ftFileTime) continue;
#ifdef UNICODE
    	if (strcmp(pcszCacheName, szName)) continue;
#else
    	if (_stricmp(pcszCacheName, pIcon->pcszName)) continue;
#endif
        if ((pfr->IconFormat != fBGR) && (pfr->IconFormat != fBGRA)) return false;

		pIcon->SetImageDimensions(pfr->ImageX, pfr->ImageY, pfr->ImageZ);

		iePImage pimTemp = ieImage::Create(iePixelFormat::BGRA, { pfr->IconXPix, pfr->IconYPix }, true, false);
		if (!pimTemp) return false;
		
		pimTemp->SetAlphaType(ieAlphaType(pfr->AlphaType));

		iePBGRA pDst4 = pimTemp->BGRA()->PixelPtr();
		DWORD nDstPitch4 = pimTemp->BGRA()->Pitch();
		DWORD dwDataOffset = DWORD(pfr->DataOffset) + (DWORD(pfr->DataOffsetHi)<<24);
		PBYTE pbData = pDATA + dwDataOffset;

		if (pfr->IconFormat == fBGRA) {
			iePBGRA pSrc4 = iePBGRA(pbData);
			DWORD nSrcPitch4 = pfr->IconXPix;
       	   	for (DWORD y = pfr->IconYPix; y--; pDst4 += nDstPitch4, pSrc4 += nSrcPitch4)
				iem_Copy(pDst4, pSrc4, nSrcPitch4*4);
			pbData = PBYTE(pSrc4);
		} else { //(pfr->IconFormat == fBGR) {
           	iePBGR pSrc3 = iePBGR(pbData);
			DWORD nSrcPitch3 = pfr->IconXPix;
   		   	for (DWORD y = pfr->IconYPix; y--; pDst4 += nDstPitch4, pSrc3 += nSrcPitch3)
				ie_BGR2BGRA(pDst4, pSrc3, nSrcPitch3);
			pbData = PBYTE(pSrc3);
		}

		pIcon->SetImage(ieImageDisplay::Create(pimTemp));
		if (!pIcon->HasImage()) { pimTemp->Release(); return false; }

		if (bMetaDataIncluded) {
			char *pcMeta = (char *)pbData;
			for (;;) {
				char cType = *pcMeta++;
				if (!cType) break;	// End of list
				if (cType == 1) {	// Comment
					TCHAR sz[2*MAX_PATH];
#ifdef UNICODE
					ie_UTF8ToUnicode(sz, pcMeta, sizeof(sz)/sizeof(TCHAR));
					pIcon->SetComment(sz);
#else
					wchar_t wsz[2*MAX_PATH];
					mdUTF8ToUnicode(wsz, pcMeta, sizeof(wsz)/sizeof(wchar_t));
					mdUnicodeToLatin1(sz, wsz, sizeof(sz));
#endif
					pIcon->SetComment(sz);
				}
				pcMeta += strlen(pcMeta)+1;
			}
		}

        return true;
    }

    return false;
}


bool FileIconCache::Load(PCTCHAR pcszPath, muiSize &whIcon)
{
	strCacheFile.SetPath(pcszPath);
	if (!strCacheFile.Length()) return false;

	Free();

	// Free any local index options
    IE_HFILE hf = ief_Open(strCacheFile.Str());

    bHaveCache = (hf != INVALID_HANDLE_VALUE);
	if (!bHaveCache) return false;

	for (;;) {

		LIST liRIFF;
		ief_Read(hf, &liRIFF, sizeof(LIST));
	    
		if ((liRIFF.ck.fcId != idRIFF) || (liRIFF.fcType != idIEIC)) {
			bHaveCache = false;
			break;
		}

		IFMT ifmt;
		iem_Zero(&ifmt, sizeof(ifmt));

   		DWORD dwPos = sizeof(liRIFF);

		while (dwPos < liRIFF.ck.dwSize) {

			ief_Seek(hf, dwPos);

			CHUNK ck;
			ief_Read(hf, &ck, sizeof(ck));

			dwPos += sizeof(ck) + ck.dwSize;

			switch (ck.fcId) {

			case idIFMT:
        		ief_Read(hf, &ifmt, sizeof(ifmt));
        		break;

			case idFREF:
        		pFREF = new BYTE[ck.dwSize];
				if (!pFREF) break;

				ief_Read(hf, pFREF, ck.dwSize);
        		break;

			case idDATA:
        		pDATA = new BYTE[ck.dwSize];
				if (!pDATA) break;

				ief_Read(hf, pDATA, ck.dwSize);

    			switch (ifmt.DataCompression) {

    			case fUncompressed:
    				break;

    			case fDeltaGIFLZW: {
        			PBYTE pbUncomp = new BYTE[ifmt.DataUncompSize];
					if (!pbUncomp) break;

        			GIFLZW_Decompress(pbUncomp, ifmt.DataUncompSize, pDATA, ck.dwSize);

					delete[] pDATA;
					pDATA = pbUncomp;

					DeltaDePredict(pDATA, ifmt.DataUncompSize);
    			}	break;

				default:
            		bHaveCache = false;
        			break;
				}
				break;
			}
		}

		iFmtIconX = ifmt.IconXPix;
		iFmtIconY = ifmt.IconYPix;

		iNumCacheRefs = ifmt.NumFREFs;
   		iSizeOfFREF = ifmt.SizeOfFREF;

		bFileNamesAreUTF8 = (ifmt.FileNamesAreUTF8 != 0);
		bMetaDataIncluded = (ifmt.MetaDataIncluded != 0);

		if (!pFREF || !pDATA || !iFmtIconX || !iFmtIconY || (whIcon.w && (whIcon.w != iFmtIconX)) || (whIcon.h && (whIcon.h != iFmtIconY))) {
    		bHaveCache = false;
			break;
		}

		whIcon.w = iFmtIconX;
		whIcon.h = iFmtIconY;

		bUpdateCache = false;
		break;
	}

	ief_Close(hf);

	return bHaveCache;
}


bool FileIconCache::Save(FileIcon *pFirstIcon, muiSize whIcon, HWND hwndNotify, UINT uMsgNotify, WPARAM wParamNotify, LPARAM lParamNotify)
{
	if (!strCacheFile.Length()) return false;

    Free();

	iFmtIconX = whIcon.w;
	iFmtIconY = whIcon.h;

	if (!pFirstIcon) {
		ief_Delete(strCacheFile.Str());
		return true;
	}

	IE_HFILE hf = CreateFile(strCacheFile.Str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_HIDDEN, NULL);
	if (hf == IE_INVALIDHFILE) return false;

	for (;;) {
		
		int		x, y, i;
		char	sz[4*MAX_PATH];
#ifndef UNICODE
		wchar_t	wsz[MAX_PATH];
#endif

		CHUNK   ckDATA;
		ckDATA.fcId = idDATA;
		
		CHUNK	ckFREF;
		ckFREF.fcId = idFREF;
		ckFREF.dwSize = 0;
		
		CHUNK	ckIFMT;
		IFMT	ifmt;
		ckIFMT.fcId = idIFMT;
		ckIFMT.dwSize = sizeof(IFMT);

		iem_Zero(&ifmt, sizeof(ifmt));
		ifmt.SizeOfFREF = sizeof(FREF);
		ifmt.IconXPix = iFmtIconX;
		ifmt.IconYPix = iFmtIconY;

		FileIcon *pEnum;

		for (pEnum = pFirstIcon; pEnum; pEnum = pEnum->NextIcon()) {
	
			if (!pEnum->HasImage()) continue;
#ifdef UNICODE
			if (!ifmt.FileNamesAreUTF8) {
				if (!ie_IsUnicode2Latin1Lossless(pEnum->GetFileNameStr())) {
					ifmt.FileNamesAreUTF8 = 1;
					if (ifmt.MetaDataIncluded) break;
				}
			}
#endif
			if (!ifmt.MetaDataIncluded) {
				if (pEnum->GetCommentStrLength()) {
					ifmt.MetaDataIncluded = 1;
					if (ifmt.FileNamesAreUTF8) break;
				}
			}
		}

		for (pEnum = pFirstIcon; pEnum; pEnum = pEnum->NextIcon()) {

			if (!pEnum->HasImage()) continue;
			iePImageDisplay pimd = pEnum->GetImage();

			ifmt.NumFREFs++;
			ifmt.DataUncompSize += (4*pimd->OrigImage()->X()*pimd->OrigImage()->Y());
#ifdef UNICODE
			if (ifmt.FileNamesAreUTF8)
				ie_UnicodeToUTF8(sz, pEnum->GetFileNameStr(), sizeof(sz));
			else
				ie_UnicodeToLatin1(sz, pEnum->GetFileNameStr(), sizeof(sz));
			
			ckFREF.dwSize += sizeof(FREF) + strlen(sz)+1;
#else
			ckFREF.dwSize += sizeof(FREF) + strlen(pEnum->GetFileNameStr())+1;
#endif
			if (ifmt.MetaDataIncluded) {
				if (pEnum->GetCommentStrLength()) {
#ifdef UNICODE
					ie_UnicodeToUTF8(sz, pEnum->GetCommentStr(), sizeof(sz));
#else
					mdLatin1ToUnicode(wsz, pEnum->GetCommentStr(), sizeof(wsz)/sizeof(wchar_t));
					mdUnicodeToUTF8(sz, wsz, sizeof(sz));
#endif
					ifmt.DataUncompSize += 1 + strlen(sz)+1;
				}
				ifmt.DataUncompSize++;
			}
		}
		
		ckFREF.dwSize = (ckFREF.dwSize+1) & (~1);	// Align on word boundary
		
		ckDATA.dwSize = (ifmt.DataUncompSize+1) & (~1); // Align on word boundary
		if (ifmt.NumFREFs == 0) break;				// Don't write empty cache files...
		
		PBYTE pbData = new BYTE[ckDATA.dwSize];
		if (!pbData) break;
		iePBGRA pDst4 = iePBGRA(pbData);
		
		for (pEnum = pFirstIcon; pEnum; pEnum = pEnum->NextIcon()) {

			if (!pEnum->HasImage()) continue;
			iePImageDisplay pimd = pEnum->GetImage();
			iePImage pim = pimd->OrigImage();

			int iPitchDst4 = pim->X();
			if (pim->BGRA()) {
				iePBGRA pSrc4 = pim->BGRA()->PixelPtr();
				int iPitchSrc4 = pim->BGRA()->Pitch();
				for (y = pim->Y(); y--; pSrc4 += iPitchSrc4, pDst4 += iPitchDst4)
					iem_Copy(pDst4, pSrc4, iPitchDst4*4);
			} else { //if (pim->CLUT()) {
				iePILUT pSrc1 = pim->CLUT()->PixelPtr();
				int iPitchSrc1 = pim->CLUT()->Pitch();
				iePBGRA pCLUT = pim->CLUT()->CLUTPtr();
				for (y = pim->Y(); y--; pSrc1 += iPitchSrc1, pDst4 += iPitchDst4)
					ie_ILUT2BGRA(pDst4, pSrc1, pCLUT, iPitchDst4);
			}

			if (ifmt.MetaDataIncluded) {
				char *pcDst = (char *)pDst4;
				if (pEnum->GetCommentStrLength()) {
					*pcDst++ = 1;
#ifdef UNICODE
					ie_UnicodeToUTF8(pcDst, pEnum->GetCommentStr(), sizeof(sz));
#else
					mdLatin1ToUnicode(wsz, pEnum->GetCommentStr(), sizeof(wsz)/sizeof(wchar_t));
					mdUnicodeToUTF8(pcDst, wsz, sizeof(sz));
#endif
					pcDst += strlen(pcDst)+1;
				}
				*pcDst++ = 0;
				pDst4 = iePBGRA(pcDst);
			}
		}

		if (pCfg->idx.bCompressCache) {	// Try to compress...
			DeltaPredict(pbData, ifmt.DataUncompSize);
			PBYTE pbComp = new BYTE[2*ifmt.DataUncompSize];
			if (!pbComp) break;
			DWORD dwCompSize = GIFLZW_Compress(pbComp, iePILUT(pbData), ifmt.DataUncompSize, 1, 0, 8);
			if (float(dwCompSize) < (float(ifmt.DataUncompSize)*0.9f)) {
				delete[] pbData;
				pbData = pbComp;
				ckDATA.dwSize = (dwCompSize+1) & (~1); // Align on word boundary
				ifmt.DataCompression = fDeltaGIFLZW;
			} else {
				delete[] pbComp;
				DeltaDePredict(pbData, ifmt.DataUncompSize);
			}
		}
		
		LIST 	liRIFF;
		liRIFF.ck.fcId = idRIFF;
		liRIFF.ck.dwSize = 4 + 8+ckIFMT.dwSize + 8+ckFREF.dwSize + 8+ckDATA.dwSize;
		liRIFF.fcType = idIEIC;
		
		ief_Write(hf, &liRIFF, 12);
		
		ief_Write(hf, &ckIFMT, 8);
		ief_Write(hf, &ifmt, sizeof(IFMT));
		
		ief_Write(hf, &ckFREF, 8);

		DWORD dwDataOffset = 0;

		for (pEnum = pFirstIcon; pEnum; pEnum = pEnum->NextIcon()) {

			if (!pEnum->HasImage()) continue;
			iePImageDisplay pimd = pEnum->GetImage();

			FREF	fref;
			fref.DataOffset = (dwDataOffset & 0x00FFFFFF);
			fref.DataOffsetHi = (dwDataOffset >> 24);
			fref.IconXPix = pimd->OrigImage()->X();
			fref.IconYPix = pimd->OrigImage()->Y();
			fref.IconFormat = fBGRA;
			fref.AlphaType = DWORD(pimd->OrigImage()->AlphaType());
			fref.ImageX = pEnum->GetImageDimensionX();
			fref.ImageY = pEnum->GetImageDimensionY();
			fref.ImageZ = pEnum->GetImageDimensionZ();
			fref.FileSize = DWORD(pEnum->GetFileSize());
			fref.FileTime = pEnum->GetFileTime();

			ief_Write(hf, &fref, sizeof(FREF));
#ifdef UNICODE
			if (ifmt.FileNamesAreUTF8) 
				ie_UnicodeToUTF8(sz, pEnum->GetFileNameStr(), sizeof(sz));
			else
				ie_UnicodeToLatin1(sz, pEnum->GetFileNameStr(), sizeof(sz));
			ief_Write(hf, sz, strlen(sz)+1);
#else
			ief_Write(hf, pEnum->GetFileNameStr(), strlen(pEnum->GetFileNameStr())+1);
#endif
			
			dwDataOffset += 4*pimd->OrigImage()->X()*pimd->OrigImage()->Y();

			if (ifmt.MetaDataIncluded) {
				if (pEnum->GetCommentStrLength()) {
#ifdef UNICODE
					ie_UnicodeToUTF8(sz, pEnum->GetCommentStr(), sizeof(sz));
#else
					mdLatin1ToUnicode(wsz, pEnum->GetCommentStr(), sizeof(wsz)/sizeof(wchar_t));
					mdUnicodeToUTF8(sz, wsz, sizeof(sz));
#endif
					dwDataOffset += 1 + strlen(sz)+1;
				}
				dwDataOffset++;
			}
		}
		if (ief_Walk(hf, 0) & 1) ief_Write(hf, &hf, 1); // Align on word boundary
		
		ief_Write(hf, &ckDATA, 8);
		ief_Write(hf, pbData, ckDATA.dwSize);
		
		delete[] pbData;

		bHaveCache = true;
		bUpdateCache = false;

		break;
	}

	ief_Close(hf);

	if (hwndNotify && uMsgNotify) {
		PostMessage(hwndNotify, uMsgNotify, wParamNotify, lParamNotify);
	}

	return true;
}


void FileIconCache::Free()
{
	iNumCacheRefs = 0;

	if (pFREF != nullptr) {
    	delete[] pFREF;
		pFREF = nullptr;
    }

	if (pDATA != nullptr) {
    	delete[] pDATA;
		pDATA = nullptr;
    }
}


void FileIconCache::Remove()
{
	if (!strCacheFile.Length()) return;

    ief_Delete(strCacheFile.Str());

    bHaveCache = false;
}


static void DeltaDePredict(PBYTE pbData, DWORD dwSrcLen)
{
	BYTE a = 0;
    while (dwSrcLen--) {
        a += *pbData;
        *pbData++ = a;
    }
}


static void DeltaPredict(PBYTE pbData, DWORD dwSrcLen)
{
	BYTE b = 0, a;
    while (dwSrcLen--) {
    	a = *pbData;
        *pbData++ = (a-b);
        b = a;
    }
}
