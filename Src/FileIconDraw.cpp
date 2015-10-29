//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	File icon drawing primitves (normal vs glass)
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

#include "FileIcon.h"


//------------------------------------------------------------------------------------------

HBRUSH CreateAlphaBrush(ieBGRA clr)
{
	struct {
		BITMAPINFOHEADER hdr;
		ieBGRA	aPixels[8 * 8];
	} bmi;

	bmi.hdr.biSize = sizeof(bmi.hdr);
	bmi.hdr.biWidth = 1;
	bmi.hdr.biHeight = 1;
	bmi.hdr.biPlanes = 1;
	bmi.hdr.biBitCount = 32;
	bmi.hdr.biCompression = BI_RGB;
	bmi.hdr.biXPelsPerMeter = 0;
	bmi.hdr.biYPelsPerMeter = 0;
	bmi.hdr.biClrUsed = 0;
	bmi.hdr.biClrImportant = 0;

	for (int i = 0; i < 8 * 8; i++)
		bmi.aPixels[i] = clr;

	return CreateDIBPatternBrushPt(&bmi, DIB_RGB_COLORS);
}


//------------------------------------------------------------------------------------------

class FileIconDrawNormal : public IFileIconDrawing {
public:
	FileIconDrawNormal(muiFont &ft_);
	virtual ~FileIconDrawNormal();
	virtual void Fill(HDC hdc, const RECT &rc, eColorList eColor);
	virtual void Frame(HDC hdc, const RECT &rc, eColorList eColor);
	virtual void Text(HDC hdc, PCTCHAR pcszText, const RECT &rc, eTextColor eColor, UINT uFlags = 0);
	virtual int GetLengthOfText(HDC hdc, PCTCHAR pcszText, int nTextChars = -1);
	virtual ieBGRA GetColor(eColorList eColor);
protected:
	muiFont	&ft;
	HBRUSH hbrBkg, hbrDef, hbrSel, hbrFoc, hbrPrs;
	COLORREF crBkg, crDef, crSel, crFoc, crPrs;
	COLORREF crFileName, crComment, crFileInfo;
	HBRUSH GetBrush(eColorList eColor);
};


IFileIconDrawing *NewFileIconDrawNormal(muiFont &ft)
{
	return new FileIconDrawNormal(ft);
}


FileIconDrawNormal::FileIconDrawNormal(muiFont &ft_)
:	IFileIconDrawing(), ft(ft_)
{
	crBkg = RGB(pCfg->idx.clrBackground.R, pCfg->idx.clrBackground.G, pCfg->idx.clrBackground.B);
	crDef = RGB(0x00,0x00,0x00);
	crSel = RGB(0x80,0x80,0x80);
	crFoc = RGB(0xCF,0xEF,0xFF);
	crPrs = RGB(153, 222, 253);

	crFileName = RGB(0,0,0);
	crComment = RGB(0,80,0);
	crFileInfo = RGB(92,92,92);

	hbrBkg = CreateSolidBrush(crBkg);
	hbrDef = CreateSolidBrush(crDef);
	hbrSel = CreateSolidBrush(crSel);
	hbrFoc = CreateSolidBrush(crFoc);
	hbrPrs = CreateSolidBrush(crPrs);

	HDC hdc = CreateCompatibleDC(NULL);
	ft.Select(hdc);
	SIZE s;
	GetTextExtentPoint32(hdc, _T("Jj|,'"), 5, &s);
	iCharYH = s.cy;
	DeleteDC(hdc);
}


FileIconDrawNormal::~FileIconDrawNormal()
{
	DeleteObject(hbrBkg);
	DeleteObject(hbrDef);
	DeleteObject(hbrSel);
	DeleteObject(hbrFoc);
	DeleteObject(hbrPrs);
}


void FileIconDrawNormal::Fill(HDC hdc, const RECT &rc, eColorList eColor)
{
	FillRect(hdc, &rc, GetBrush(eColor));
}


void FileIconDrawNormal::Frame(HDC hdc, const RECT &rc, eColorList eColor)
{
	FrameRect(hdc, &rc, GetBrush(eColor));
}


int FileIconDrawNormal::GetLengthOfText(HDC hdc, PCTCHAR pcszText, int nTextChars)
{
	if (!pcszText || !*pcszText) return 0;
	if (nTextChars < 0) nTextChars = _tcslen(pcszText);

	ft.Select(hdc);

	SIZE size;
	if (!GetTextExtentPoint32(hdc, pcszText, nTextChars, &size))
		return nTextChars*8;

	return size.cx;
}


void FileIconDrawNormal::Text(HDC hdc, PCTCHAR pcszText, const RECT &rc, eTextColor eColor, UINT uFlags)
{
	if (!pcszText || !*pcszText) return;

	COLORREF cr;
	switch (eColor) {
	case eFileName:	cr = crFileName;	break;
	case eComment:	cr = crComment;		break;
	case eFileInfo:	cr = crFileInfo;	break;
	default:		cr = RGB(0,0,0);	break;
	}

	ft.Select(hdc);
	SetBkColor(hdc, crBkg);
	SetBkMode(hdc, OPAQUE);
	SetTextColor(hdc, cr);

	DrawText(hdc, pcszText, -1, (RECT *)&rc, uFlags);
}


HBRUSH FileIconDrawNormal::GetBrush(eColorList eColor)
{
	switch (eColor) {
	case eClear:		return hbrBkg;
	case eDefault:		return hbrDef;
	case eSelected:		return hbrSel;
	case eFocused:		return hbrFoc;
	default:			return hbrPrs;
	}
}


ieBGRA FileIconDrawNormal::GetColor(eColorList eColor)
{
	COLORREF cr;
	switch (eColor) {
	case eClear:		cr = crBkg;	break;
	case eDefault:		cr = crDef;	break;
	case eSelected:		cr = crSel;	break;
	case eFocused:		cr = crFoc;	break;
	default:			cr = crPrs;	break;
	}
	return ieBGRA(GetBValue(cr), GetGValue(cr), GetRValue(cr));
}


//------------------------------------------------------------------------------------------

class FileIconDrawGlass : public IFileIconDrawing {
public:
	FileIconDrawGlass(muiFont &ft_);
	virtual ~FileIconDrawGlass();
	virtual void Fill(HDC hdc, const RECT &rc, eColorList eColor);
	virtual void Frame(HDC hdc, const RECT &rc, eColorList eColor);
	virtual void Text(HDC hdc, PCTCHAR pcszText, const RECT &rc, eTextColor eColor, UINT uFlags = 0);
	virtual int GetLengthOfText(HDC hdc, PCTCHAR pcszText, int nTextChars = -1);
	virtual ieBGRA GetColor(eColorList eColor);
protected:
	muiFont	&ft;
	HBRUSH hbrBkg, hbrDef, hbrSel, hbrFoc, hbrPrs;
	ieBGRA clrBkg, clrDef, clrSel, clrFoc, clrPrs;
	ieBGRA clrFileName, clrComment, clrFileInfo;
	HDC hdcTextDIB;
	HBITMAP hbmpTextDIB, hbmpOld;
	iePBGRA pTextDIB;
	int iTextDIBXW, iTextDIBYH;
	void CreateTextDIB(int nMinXW, int nMinYH);
	void FreeTextDIB();
	HBRUSH GetBrush(eColorList eColor);
};


IFileIconDrawing *NewFileIconDrawGlass(muiFont &ft)
{
	return new FileIconDrawGlass(ft);
}


FileIconDrawGlass::FileIconDrawGlass(muiFont &ft_)
:	IFileIconDrawing(), ft(ft_), hbmpTextDIB(NULL)
{
	clrBkg = ieBGRA(0,0,0,0x80);
	clrDef = ieBGRA(0x00,0x00,0x00);
	clrSel = ieBGRA(0x80,0x80,0x80);
	clrFoc = ieBGRA(0xE0,0xC0,0xA0);
	clrPrs = ieBGRA(253, 222, 153);

	clrFileName = ieBGRA(255,255,255);
	clrComment = ieBGRA(0,128,0);
	clrFileInfo = ieBGRA(160,160,160);

	hbrBkg = CreateAlphaBrush(clrBkg);
	hbrDef = CreateAlphaBrush(clrDef);
	hbrSel = CreateAlphaBrush(clrSel);
	hbrFoc = CreateAlphaBrush(clrFoc);
	hbrPrs = CreateAlphaBrush(clrPrs);

	hdcTextDIB = CreateCompatibleDC(NULL);
	CreateTextDIB(256, 20);

	SIZE s;
	GetTextExtentPoint32(hdcTextDIB, _T("Jj|,'"), 5, &s);
	iCharYH = s.cy;
}


FileIconDrawGlass::~FileIconDrawGlass()
{
	DeleteObject(hbrBkg);
	DeleteObject(hbrDef);
	DeleteObject(hbrSel);
	DeleteObject(hbrFoc);
	DeleteObject(hbrPrs);

	FreeTextDIB();
	DeleteDC(hdcTextDIB);
}


void FileIconDrawGlass::CreateTextDIB(int nMinXW, int nMinYH)
{
	if (!hdcTextDIB) return;
	if (hbmpTextDIB) FreeTextDIB();

	iTextDIBXW = (max(nMinXW, 1)+15) & (~15);
	iTextDIBYH = max(nMinYH, 1);
	
    struct {
	    BITMAPINFOHEADER	bmih;
    } bmi;
	ZeroMemory(&bmi.bmih, sizeof(bmi.bmih));
    bmi.bmih.biSize = sizeof(bmi.bmih);
   	bmi.bmih.biWidth = iTextDIBXW;
	bmi.bmih.biHeight = -iTextDIBYH;
   	bmi.bmih.biPlanes = 1;
	bmi.bmih.biBitCount = 32;
	bmi.bmih.biCompression = BI_RGB;
	bmi.bmih.biClrUsed = 0;

	hbmpTextDIB = CreateDIBSection(hdcTextDIB, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&pTextDIB, NULL, 0);
	if (!hbmpTextDIB) return;

	hbmpOld = (HBITMAP)SelectObject(hdcTextDIB, hbmpTextDIB);

	ft.Select(hdcTextDIB);
	SetBkColor(hdcTextDIB, RGB(0,0,0));
	SetBkMode(hdcTextDIB, OPAQUE);
	SetTextColor(hdcTextDIB, RGB(255,255,255));
}


void FileIconDrawGlass::FreeTextDIB()
{
	if (!hbmpTextDIB) return;

	SelectObject(hdcTextDIB, hbmpOld);
	DeleteObject(hbmpTextDIB);
	hbmpTextDIB = NULL;
}


void FileIconDrawGlass::Fill(HDC hdc, const RECT &rc, eColorList eColor)
{
	FillRect(hdc, &rc, GetBrush(eColor));
}


void FileIconDrawGlass::Frame(HDC hdc, const RECT &rc, eColorList eColor)
{
	FrameRect(hdc, &rc, GetBrush(eColor));
}


int FileIconDrawGlass::GetLengthOfText(HDC hdc, PCTCHAR pcszText, int nTextChars)
{
	if (!pcszText || !*pcszText) return 0;
	if (nTextChars < 0) nTextChars = _tcslen(pcszText);

	SIZE size;
	if (!GetTextExtentPoint32(hdcTextDIB, pcszText, nTextChars, &size))
		return nTextChars*8;

	return size.cx;
//	RECT rc = { 0, 0, 65535, 65535 };
//	if (!DrawText(hdcTextDIB, pcszText, nTextChars, &rc, DT_TOP | DT_LEFT | DT_SINGLELINE | DT_CALCRECT))
//	return rc.right;
}


void FileIconDrawGlass::Text(HDC hdc, PCTCHAR pcszText, const RECT &rc, eTextColor eColor, UINT uFlags)
{
	if (!pcszText || !*pcszText) return;

	// Find out actual size of text
	int nChars = _tcslen(pcszText);
	uFlags |= DT_NOCLIP;

	int iX = rc.left;
	int iY = rc.top;
	int iXW = (rc.right - iX);
	int iYH = (rc.bottom - iY);

	RECT rcMin = rc;
	if (DrawText(hdcTextDIB, pcszText, nChars, &rcMin, uFlags | DT_CALCRECT)) {
		int iMinXW = rcMin.right - rcMin.left;
		int iMinYH = rcMin.bottom - rcMin.top;
		if (iMinXW < iXW) {
			if (uFlags & DT_CENTER) {
				iX += (iXW - iMinXW)/2;
				uFlags &= ~DT_CENTER;
			} else if (uFlags & DT_RIGHT) {
				iX += (iXW - iMinXW);
				uFlags &= ~DT_RIGHT;
			}
			iXW = iMinXW;
		}
		if (iMinYH < iYH) {
			if (uFlags & DT_SINGLELINE) {
				if (uFlags & DT_VCENTER) {
					iY += (iYH - iMinYH)/2;
					uFlags &= ~DT_VCENTER;
				} else if (uFlags & DT_BOTTOM) {
					iY += (iYH - iMinYH);
					uFlags &= ~DT_BOTTOM;
				}
			}
			iYH = iMinYH;
		}
	}

	iXW += 2;	// NB: +2 'cause we want an extra pixel at the border so that the font smoothing will look bette!
	iYH += 2;

	// Ensure we have a big enough DIB to draw the text to
	if ((iXW > iTextDIBXW) || (iYH > iTextDIBYH)) CreateTextDIB(iXW, iYH);
	if (!hbmpTextDIB) return;

	// Select color
	ieBGRA clr;
	switch (eColor) {
	case eFileName:	clr = clrFileName;		break;
	case eComment:	clr = clrComment;		break;
	case eFileInfo:	clr = clrFileInfo;		break;
	default:		clr = ieBGRA(0,0,0);	break;
	}
	clr.A = 0xFF - clrBkg.A;

	// Draw the text to in-memory DIB
	RECT rcTextDIB = { 0, 0, iXW, iYH };
	FillRect(hdcTextDIB, &rcTextDIB, hbrBkg);

	rcTextDIB.left++;
	rcTextDIB.top++;

	DrawText(hdcTextDIB, pcszText, nChars, &rcTextDIB, uFlags);

	// Modify DIB:
#ifndef __X64__
	if (g_bSSE2) 
#endif
	{
		__m128i r0, r1, r2, r3, r4, r5, r6, r7;

		r7 = _mm_setzero_si128();									// 0
		r6 = _mm_set1_epi32(clr.dw);								// CA  CR  CG  CB  CA  CR  CG  CB  CA  CR  CG  CB  CA  CR  CG  CB
		r6 = _mm_unpacklo_epi8(r7, r6);								// CA<<8   CR<<8   CG<<8   CB<<8   CA<<8   CR<<8   CG<<8   CB<<8
		r5 = _mm_set1_epi16(1);										// 1       1       1       1       1       1       1       1
		r4 = _mm_set1_epi32(0xFF);									// FF              FF              FF              FF
		r3 = _mm_set1_epi32(clrBkg.dw);								// DA  0   0   0   DA  0   0   0   DA  0   0   0   DA  0   0   0

		ieBGRA *py = pTextDIB;
		for (int y = iYH; y--; py += iTextDIBXW) {
			ieBGRA *px = py;

			for (int x_4 = (iXW+3)>>2; x_4--; px += 4) {

				r0 = _mm_load_si128((__m128i *)px);
				r1 = r0;
				r2 = r0;											// X3  R3  G3  B3  X2  R2  G2  B2  X1  R1  G1  B1  X0  R0  G0  B0 
				r0 = _mm_srli_epi32(r0, 16);						// 0   0   X3  R3  0   0   X2  R2  0   0   X1  R1  0   0   X0  R0 
				r1 = _mm_srli_epi32(r1, 8);							// 0   X3  R3  G3  0   X2  R2  G2  0   X1  R1  G1  0   X0  R0  G0 
				r0 = _mm_max_epu8(r0, r2);
				r0 = _mm_max_epu8(r0, r1);							// x   x   x   A3  x   x   x   A2  x   x   x   A1  x   x   x   A0
				r0 = _mm_and_si128(r0, r4);							// 0       A3      0       A2      0       A1      0       A0
				r0 = _mm_shufflelo_epi16(r0, _MM_SHUFFLE(2,2,0,0));
				r0 = _mm_shufflehi_epi16(r0, _MM_SHUFFLE(2,2,0,0));	// A3      A3      A2      A2      A1      A1      A0      A0
				r1 = r0;
				r0 = _mm_unpacklo_epi32(r0, r0);					// A1      A1      A1      A1      A0      A0      A0      A0
				r1 = _mm_unpackhi_epi32(r1, r1);					// A3      A3      A3      A3      A2      A2      A2      A2
				r0 = _mm_add_epi16(r0, r5);							// A1'     A1'     A1'     A1'     A0'     A0'     A0'     A0' 
				r1 = _mm_add_epi16(r1, r5);							// A3'     A3'     A3'     A3'     A2'     A2'     A2'     A2' 
				r0 = _mm_mulhi_epu16(r0, r6);						// xA1"    xR1     xG1     xB1     xA0"    xR0     xG0     xB0
				r1 = _mm_mulhi_epu16(r1, r6);						// xA3"    xR3     xG3     xB3     xA2"    xR2     xG2     xB2
				r0 = _mm_packus_epi16(r0, r1);						// xA3"xR3 xG3 xB3 xA2"xR2 xG2 xB2 xA1"xR1 xG1 xB1 xA0"xR0 xG0 xB0
				r0 = _mm_adds_epu8(r0, r3);							// xA3 xR3 xG3 xB3 xA2 xR2 xG2 xB2 xA1 xR1 xG1 xB1 xA0 xR0 xG0 xB0
				_mm_store_si128((__m128i *)px, r0);
			}
		}
	}
#ifndef __X64__
	else {

		ieBGRA *py = pTextDIB;
		for (int y = iYH; y--; py += iTextDIBXW) {
			ieBGRA *px = py;

			for (int x = iXW; x--; px++) {
				int A = 1 + max(max(px->B, px->G), px->R);
				px->B = BYTE( (clr.B*A) >> 8);
				px->G = BYTE( (clr.G*A) >> 8);
				px->R = BYTE( (clr.R*A) >> 8);
				px->A = BYTE(((clr.A*A) >> 8) + clrBkg.A);
			}
		}
	}
#endif

	// Copy to destination
	BitBlt(hdc, iX-1, iY-1, iXW, iYH, hdcTextDIB, 0, 0, SRCCOPY);
}


HBRUSH FileIconDrawGlass::GetBrush(eColorList eColor)
{
	switch (eColor) {
	case eClear:		return hbrBkg;
	case eDefault:		return hbrDef;
	case eSelected:		return hbrSel;
	case eFocused:		return hbrFoc;
	default:			return hbrPrs;
	}
}


ieBGRA FileIconDrawGlass::GetColor(eColorList eColor)
{
	switch (eColor) {
	case eClear:		return clrBkg;
	case eDefault:		return clrDef;
	case eSelected:		return clrSel;
	case eFocused:		return clrFoc;
	default:			return clrPrs;
	}
}
