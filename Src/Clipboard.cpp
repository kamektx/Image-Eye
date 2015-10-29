//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Windows clipboard copy/paste of bitmap
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


//-----------------------------------------------------------------------------

bool Clipboard_Write(HWND hwnd, iePImage pim)
// Copy image to clipboard
{
	if (!OpenClipboard(hwnd)) return false;

	EmptyClipboard();

	// Write to a DIB in global memory
	PBYTE pbImage;
	int iPixelSize, iSrcPitch;	
	iePBGRA pCLUT;
	if (pim->CLUT()) {
		iPixelSize = 1;
		iSrcPitch = pim->CLUT()->Pitch();
		pbImage = PBYTE(pim->CLUT()->PixelPtr());
		pCLUT = pim->CLUT()->CLUTPtr();
	} else { //if (pimOrig->BGRA()) {
		iPixelSize = 4;
		iSrcPitch = pim->BGRA()->Pitch();
		pbImage = PBYTE(pim->BGRA()->PixelPtr());
		pCLUT = nullptr;
	}

	bool bHasAlpha = (!pCLUT) && (pim->AlphaType() != ieAlphaType::None);
	DWORD dwDstBPL = (pim->X()*(pCLUT ? 1 : (bHasAlpha ? 4 : 3)) + 3) & (~3);
	DWORD dwSize = sizeof(BITMAPINFOHEADER);
	if (pCLUT) dwSize += 256*sizeof(RGBQUAD);
    dwSize += pim->Y()*dwDstBPL;

    HGLOBAL hDIB = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, dwSize);
    PBYTE	pDIB = (PBYTE)GlobalLock(hDIB);

    BITMAPINFOHEADER *pbmih = (BITMAPINFOHEADER *)pDIB;
	pbmih->biSize = sizeof(BITMAPINFOHEADER);
	pbmih->biWidth = pim->X();
	pbmih->biHeight = pim->Y();
	pbmih->biPlanes = 1;
	pbmih->biBitCount = pCLUT ? 8 : (bHasAlpha ? 32 : 24);
	pbmih->biCompression = BI_RGB;
	pbmih->biSizeImage = 0;
	pbmih->biXPelsPerMeter = pbmih->biYPelsPerMeter = 0;
	pbmih->biClrUsed = pbmih->biClrImportant = 0;

    PBYTE	pbDst = PBYTE(pbmih+1);

    if (pCLUT) {

		iem_Copy(pbDst, pCLUT, 256*sizeof(ieBGRA));
		pbDst += 256*sizeof(ieBGRA);

		PBYTE pSrc = pbImage + pim->Y()*iSrcPitch;

		for (int i = pim->Y(); i--; pbDst += dwDstBPL) {
			pSrc -= iSrcPitch;
		    iem_Copy(pbDst, pSrc, dwDstBPL);
		}

    } else {

	    iePBGRA pSrc = iePBGRA(pbImage) + pim->Y()*iSrcPitch;

		for (int i = pim->Y(); i--; pbDst += dwDstBPL) {
			pSrc -= iSrcPitch;
			if (bHasAlpha) iem_Copy(pbDst, pSrc, pim->X() * 4);
			else ie_BGRA2BGR(iePBGR(pbDst), pSrc, pim->X());
		}
	}

    GlobalUnlock(hDIB);

	// Hand over to clipboard
	bool bOk = (SetClipboardData(CF_DIB, hDIB) != NULL);    
	if (!bOk) GlobalFree(hDIB);
		
	CloseClipboard();
	return bOk;
}


bool Clipboard_Query()
// Is there a DIB on the clipboard?
{
	return IsClipboardFormatAvailable(CF_DIB);
}


bool Clipboard_Read(ieIViewer &Viewer)
// Paste image from clipboard

{
	HANDLE	hf;
	TCHAR	szFile[MAX_PATH];
	HANDLE	hData = NULL;
	void *	pData = nullptr;
	HWND	hwnd = Viewer.GetHWnd();
	
	if (!Clipboard_Query()) return false;
	if (!OpenClipboard(hwnd)) return false;
	
	bool r = true;

	if (r) r = (hData = GetClipboardData(CF_DIB)) != NULL;
	if (r) r = (pData = GlobalLock(hData)) != nullptr;
	
	if (r) {
		// Write the DIB to disk, then use bmp-reader to load it
		BITMAPINFOHEADER *pbmi;
		pbmi = (BITMAPINFOHEADER *)pData;

		if (GetTempPath(MAX_PATH, szFile) == 0) _tcscpy(szFile, _T("C:\\"));
		PTCHAR pszFile = szFile + _tcslen(szFile);
		if ((pszFile[-1] != '\\') && (pszFile[-1] != '/')) *pszFile++ = '\\';
		_tcscpy(pszFile, _T("ieclipb.bmp"));

		WORD	bfType; // Note! Treat this WORD separately or it'll ruin DWORD alignment
		struct {			
			DWORD	bfSize;
			WORD	bfRes1;
			WORD	bfRes2;
			DWORD	bfOffset;
		} bmfh;

	    bfType = 0x4D42;
	    
		bmfh.bfRes1 = bmfh.bfRes2 = 0;

		int iColors = 0;
		if (pbmi->biBitCount <= 8) {
			iColors = pbmi->biClrUsed;
			if (iColors == 0) iColors = (1<<pbmi->biBitCount);
		}
		DWORD dwSize = pbmi->biSizeImage;
		if (dwSize == 0) dwSize = pbmi->biHeight * ((((pbmi->biWidth*pbmi->biBitCount+7)/8)+3) & (~3));

		bmfh.bfOffset = sizeof(bfType)+sizeof(bmfh) + pbmi->biSize + iColors * sizeof(RGBQUAD);
		if (pbmi->biCompression == BI_BITFIELDS) bmfh.bfOffset += 3 * sizeof(DWORD);
		bmfh.bfSize = bmfh.bfOffset + dwSize;

		r = (hf = ief_Open(szFile, ieCreateNewFile)) != IE_INVALIDHFILE;

		if (r) {
			ief_Write(hf, &bfType, sizeof(bfType));
			ief_Write(hf, &bmfh, sizeof(bmfh));
			ief_Write(hf, pData, bmfh.bfSize - (sizeof(bfType)+sizeof(bmfh)));
			ief_Close(hf);
		}
	}

	if (r) r = Viewer.LoadNewImage(szFile);

	ief_Delete(szFile);	
	if (hData) GlobalUnlock(hData);
	CloseClipboard();

	return r;
}
