//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	File icons (thumbnails)
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

#include <shellapi.h>
#include "mdSystem.h"
#include "FileIcon.h"
#include "../Res/resource.h"


//------------------------------------------------------------------------------------------

void ieString::Set(PCTCHAR pcszNewStr, bool bStoreAsReference)
{
	if (!pcszNewStr) pcszNewStr = _T("");
	int nNewLength = int(_tcslen(pcszNewStr));

	PTCHAR pszOldAlloc = pszAlloc;
	nLength = 0;

	if (bStoreAsReference) {
		pszAlloc = nullptr;
		pcszStr = pcszNewStr;
		nLength = nNewLength;
	}
	else {
		pszAlloc = new TCHAR[nNewLength + 1];
		if (pszAlloc) {
			memcpy(pszAlloc, pcszNewStr, (nNewLength + 1)*sizeof(TCHAR));
			pcszStr = pszAlloc;
			nLength = nNewLength;
		}
		else {
			pcszStr = _T("");
		}
	}

	if (pszOldAlloc) delete[] pszOldAlloc;
}


bool ieFileString::Set(PCTCHAR pcszNewFileStr)
{
	if (!pcszNewFileStr || !*pcszNewFileStr) {
		*szFile = 0;
		ieString::Set(szFile, true);
		pcszName = pcszExt = szFile;
		return true;
	}

	int nChars = int(_tcslen(pcszNewFileStr)) + 1;
	if (nChars > MAX_PATH) { pcszNewFileStr = _T("<error - too long path>"); nChars = 24; }

	memcpy(szFile, pcszNewFileStr, nChars*sizeof(TCHAR));

	ieString::Set(szFile, true);

	ief_SplitPath(szFile, pcszName, pcszExt);

	if (!*pcszName && (szFile[0] && (szFile[1] == ':') && (!szFile[2] || ((szFile[2] == '\\') && !szFile[3]))))
		pcszName = szFile;	// Special case of "X:" or "X:\\"

	return true;
}


bool ieFileString::SetPath(PCTCHAR pcszNewPath)
{
	int nNewPathLength = int(_tcslen(pcszNewPath));
	bool bAddSlash = nNewPathLength && (pcszNewPath[nNewPathLength - 1] != '\\') && (pcszNewPath[nNewPathLength - 1] != '/');
	if (bAddSlash) nNewPathLength++;

	int nOldPathLength = PathLength();
	int iDeltaPathLength = nNewPathLength - nOldPathLength;

	int nNameLength = NameAndExtLength();

	if (nNewPathLength + nNameLength + 1 > MAX_PATH) return false;

	if (iDeltaPathLength) {
		iem_Move(szFile + nNewPathLength, szFile + nOldPathLength, (nNameLength + 1)*sizeof(TCHAR));
		pcszName += iDeltaPathLength;
		pcszExt += iDeltaPathLength;
		nLength += iDeltaPathLength;
	}

	if (nNewPathLength) {
		iem_Copy(szFile, pcszNewPath, nNewPathLength*sizeof(TCHAR));
		if (bAddSlash) szFile[nNewPathLength - 1] = '\\';
	}

	return true;
}


bool ieFileString::SetNameAndExt(PCTCHAR pcszNewName)
{
	int nNewNameLength = int(_tcslen(pcszNewName));
	int nPathLength = PathLength();

	if (nPathLength + nNewNameLength + 1 > MAX_PATH) return false;

	iem_Copy(szFile + nPathLength, pcszNewName, (nNewNameLength + 1)*sizeof(TCHAR));
	nLength = nPathLength + nNewNameLength;

	pcszExt = szFile + nLength;
	while (pcszExt >= pcszName)
		if (*pcszExt == '.')
			break;
		else
			pcszExt--;
	if (pcszExt < pcszName)
		pcszExt = szFile + nLength;

	return true;
}


bool ieFileString::IsExt(PCTCHAR pcszTestExt) const
{
	if (!pcszTestExt || !*pcszTestExt) return false;

	PCTCHAR pcsz = pcszExt;
	if ((*pcszTestExt != '.') && *pcsz) pcsz++;

	for (;;) {
		TCHAR c0 = *pcsz++;
		TCHAR c1 = *pcszTestExt++;
		if ((c0 >= 'a') && (c0 <= 'z')) c0 += 'A' - 'a';
		if ((c1 >= 'a') && (c1 <= 'z')) c1 += 'A' - 'a';
		if (c0 != c1) return false;
		if (!c0) return true;
	}
}


//------------------------------------------------------------------------------------------

FileIcon::FileIcon(PCTCHAR pcszFile, QWORD qwFileSize, QWORD qwFileTime, bool bIsDirectory)
	: pPrev(nullptr), pNext(nullptr), FileInfo(pcszFile), ImageInfo(), IconPlacement(), IconState()
{

	// Set info
    Type = (bIsDirectory) ? fitDirectory : fitUndecided;

	SetFileSize(qwFileSize);
	SetFileTime(qwFileTime);

	if (!bIsDirectory && IsFileExt(_T(".ies")))
    	Type = fitSlideshow;
}


FileIcon::~FileIcon()
{
	UnlinkIcon();

    if (ImageInfo.hWinIcon) DestroyIcon(ImageInfo.hWinIcon);
    if (ImageInfo.pimd) ImageInfo.pimd->Release();
}


void iePrintNiceFileSize(PTCHAR pszFileSize, QWORD qwFileSize)
{
	if (qwFileSize < 1024) {
		_stprintf(pszFileSize, _T("%d Bytes"), DWORD(qwFileSize));
	} else if (qwFileSize < 1024*1204) {
		DWORD dw = DWORD((10*qwFileSize + 511)/1024);
		_stprintf(pszFileSize, _T("%d.%d KB"), dw/10, dw%10);
	} else {
		DWORD dw = DWORD((10*qwFileSize + 511*1024)/(1024*1024));
		_stprintf(pszFileSize, _T("%d.%d MB"), dw/10, dw%10);
	}
}


void FileIcon::SetFileSize(QWORD qwFileSize)
{ 
	FileInfo.qwSize = qwFileSize; 

	if (Type == fitDirectory) {
		*FileInfo.szFileSize = 0;
	} else {
		iePrintNiceFileSize(FileInfo.szFileSize, FileInfo.qwSize);
	}
}


void FileIcon::SetFileTime(IE_FILETIME ftFileTime)
{ 
	FileInfo.ftTimeStamp = ftFileTime; 

	if (!FileInfo.ftTimeStamp) {

		*FileInfo.szFileTime = 0;

	} else {

		ie_PrintFileTime(FileInfo.szFileTime, FileInfo.ftTimeStamp);
	}
}


void FileIcon::SetImageDimensions(int iX_, int iY_, int iZ_)
{
	Type = fitImage;

	ImageInfo.iX = iX_;
	ImageInfo.iY = iY_;
	ImageInfo.iZ = iZ_;

	_stprintf(ImageInfo.szDimensions, _T("%dx%dx%d"), ImageInfo.iX, ImageInfo.iY, ImageInfo.iZ);
}


void FileIcon::Draw(HDC hdc, IFileIconDrawing *pDraw, int iScrollY, int iWindowYH)
{
	// Get drawing origio + clip to window
	if (!IconPlacement.bVisible) return;

	muiCoord xy = IconPlacement.xyPos;
	xy.y -= iScrollY;
	if ((xy.y > iWindowYH) || ((xy.y + IconPlacement.whSize.h - 16) <= 0)) return;

	//
	// Draw icon / thumbnail image
	//

	// Find out size of icon / thumbnail image
	muiSize whIcon;
	bool bCenterIcon;
	GetIconWH(whIcon, bCenterIcon);

    if ((Type != fitImage) && (Type != fitUndecided) && !ImageInfo.hWinIcon && !ImageInfo.bTriedWinIcon) {

		// Delayed loading of icons:
		PCTCHAR pcszName = FileInfo.File.NameStr();

		if (Type == fitDirectory) {
			// Directory: Use icons from our resources
			if ((pcszName[0] == '.') && (pcszName[1] == '.') && (pcszName[2] == 0)) {
				static HICON hClosedFolderIco = NULL;
				if (!hClosedFolderIco) hClosedFolderIco = (HICON)LoadImage(g_hInst, MAKEINTRESOURCE(FOLDERCLOSEDICO), IMAGE_ICON, 0, 0, LR_SHARED);
				ImageInfo.hWinIcon = hClosedFolderIco;
			} else if (FileInfo.File.Length() > 3) {	// Only do non-root (drive) directories...
				static HICON hOpenFolderIco = NULL;
				if (!hOpenFolderIco) hOpenFolderIco = (HICON) LoadImage(g_hInst, MAKEINTRESOURCE(FOLDEROPENICO), IMAGE_ICON, 0, 0, LR_SHARED);
				ImageInfo.hWinIcon = hOpenFolderIco;
			}
		} else if ((pcszName[0] >= 'A') && (pcszName[0] <= 'B') && (pcszName[1] == ':') && ((pcszName[2] == '\\') || (pcszName[2] == '/')) && (pcszName[3] == 0)) {
			// Floppy drive: Retrieve icon from shell32.dll - don't ask Windows to retrieve it since that will attempt to access the drive!
			static HICON hFloppyIco = NULL;
			if (!hFloppyIco) hFloppyIco = ExtractIcon(NULL, _T("shell32.dll"), 6);
			ImageInfo.hWinIcon = hFloppyIco;
		}
		if (!ImageInfo.hWinIcon) {
			// Ask Windows for an icon...
			SHFILEINFO sfi;
			SHGetFileInfo(GetFileStr(), 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_LARGEICON);
			ImageInfo.hWinIcon = sfi.hIcon;
			//TCHAR szPath[MAX_PATH];
			//_tcscpy(szPath, GetFileStr());
			//WORD w = 1;
			//ImageInfo.hWinIcon = ExtractAssociatedIcon(NULL, szPath, &w);
		}

		ImageInfo.bTriedWinIcon = true;
	}

	RECT rcIcon;	// Icon rectangle
    rcIcon.left = xy.x + 1;	//NB: +1 is due to the border frame
	rcIcon.top = xy.y + 1;
	if (bCenterIcon) rcIcon.left += (IconPlacement.whMaxIcon.w-whIcon.w)/2;
	rcIcon.top += (IconPlacement.whMaxIcon.h-whIcon.h)/2 + 2;
	rcIcon.right = rcIcon.left + whIcon.w;
    rcIcon.bottom = rcIcon.top + whIcon.h;

    // Draw border frame?
	bool bNonAlphaImage = (Type == fitImage);
	if (bNonAlphaImage && ImageInfo.pimd && (ImageInfo.pimd->OrigImage()->AlphaType() != ieAlphaType::None))
		bNonAlphaImage = false;

	IFileIconDrawing::eColorList eStateClr = IconState.bPressed ? IFileIconDrawing::ePressed : (IconState.bFocused ? IFileIconDrawing::eFocused : (IconState.bSelected ? IFileIconDrawing::eSelected : (bNonAlphaImage ? IFileIconDrawing::eDefault : IFileIconDrawing::eClear)));

	if (ImageInfo.pimd) {

		if (eStateClr != IFileIconDrawing::eClear)
			ImageInfo.pimd->SetOutline(pDraw->GetColor(eStateClr));
		else 
			ImageInfo.pimd->DisableOutline();

	} else {
		int nDist = bCenterIcon ? 3 : 1;

		RECT rcFrame = rcIcon;
		rcFrame.left -= nDist;
		rcFrame.top -= nDist;
		rcFrame.right += nDist;
		rcFrame.bottom += nDist;

		if (!ImageInfo.pimd && IconState.bSelected) pDraw->Fill(hdc, rcFrame, IFileIconDrawing::eSelected);

		pDraw->Frame(hdc, rcFrame, eStateClr);
	}

    // Draw icon / thumbnail image
    if (ImageInfo.pimd) {
		ImageInfo.pimd->DrawToDC(hdc, { rcIcon.left, rcIcon.top }, { 0, 0 }, { IE_WH_DEFAULT, IE_WH_DEFAULT }, IconState.bSelected ? 0x80 : 0xFF);
    } else if (ImageInfo.hWinIcon) {
		DrawIconEx(hdc, rcIcon.left, rcIcon.top, ImageInfo.hWinIcon, 0, 0, 0, NULL, DI_NORMAL);
	}

	//
	// Draw texts
	//

	// Find out text area
	RECT rcText;
	rcText.left = xy.x;
	rcText.right = rcText.left + IconPlacement.whMaxIcon.w + 3;
    rcText.top = rcIcon.bottom + 4;
    int iMaxY = min(xy.y + IconPlacement.whSize.h, iWindowYH);
	if (rcText.top >= iMaxY) return;

	IndexOptions *pidx = &pCfg->idx;
	int nLineHeight = pDraw->GetCharHeight();

	int nNameLines = pidx->cShowNameLines ? pidx->cShowNameLines : 0;
	int nCommentLines = pidx->bShowComment ? 1 : 0;
	int nInfoLines = (pidx->bShowRes ? 1 : 0) + (pidx->bShowSize ? 1 : 0) + (pidx->bShowDate ? 1 : 0);

	// Draw name

	if (nNameLines && nCommentLines && (Type == fitImage) && pidx->bShowComment && !GetCommentStrLength()) {
		nNameLines++;
		nCommentLines--;
	}

	if (nNameLines) {
		rcText.bottom = min(rcText.top + nNameLines*nLineHeight, iMaxY);

		int nNameLen = GetFileNameWithoutExtStrLength();
		if (nNameLen) {

			PCTCHAR pcszName = GetFileNameStr();

			PCTCHAR pcNameEnd = pcszName + nNameLen;
			bool bAddLinkSymbol = (pcNameEnd[0] == '.') && ((pcNameEnd[1] == 'l') || (pcNameEnd[1] == 'L')) && ((pcNameEnd[2] == 'n') || (pcNameEnd[2] == 'N')) && ((pcNameEnd[3] == 'k') || (pcNameEnd[3] == 'K'));

			if (pidx->bShowExt || (Type != fitImage)) {
				nNameLen += _tcslen(pcszName+nNameLen);
				 if (bAddLinkSymbol) nNameLen -= 4;
			} else if (bAddLinkSymbol) {
				while ((--pcNameEnd >= pcszName) && (*pcNameEnd != '.'))
				{}
				if (pcNameEnd > pcszName) nNameLen = (pcNameEnd - pcszName);
			}

			int nUnusedLines = nNameLines;
			
			DrawLongText(hdc, pDraw, rcText, DT_TOP | DT_WORD_ELLIPSIS | DT_NOPREFIX, IFileIconDrawing::eFileName, pcszName, nNameLen, nUnusedLines, bAddLinkSymbol);
			
			if (nUnusedLines && nCommentLines && (Type == fitImage)) {
				nCommentLines += nUnusedLines; 
			}
			rcText.bottom -= nUnusedLines*nLineHeight;
			rcText.bottom += 5;
		}

		rcText.top = rcText.bottom;
		if (rcText.top >= iMaxY) return;
	}

	// Draw comment
	if (pidx->bShowComment && (Type == fitImage)) {
		rcText.bottom = min(rcText.top +  nCommentLines*nLineHeight, iMaxY);

		int nCommentLen = GetCommentStrLength();
		if (nCommentLen) {

			PCTCHAR pcszComment = GetCommentStr();

			int nUnusedLines = nCommentLines;

			DrawLongText(hdc, pDraw, rcText, DT_TOP | DT_WORD_ELLIPSIS | DT_NOPREFIX, IFileIconDrawing::eComment, pcszComment, nCommentLen, nUnusedLines, false);

			rcText.bottom -= nUnusedLines*nLineHeight;
			rcText.bottom += 5;
		}

		rcText.top = rcText.bottom;
		if (rcText.top >= iMaxY) return;
	}

	// Draw resolution, size, date
	if (nInfoLines) {
	    
		rcText.bottom = min(rcText.top + nInfoLines*nLineHeight, iMaxY);

		TCHAR sz[MAX_PATH], *p = sz;

		if (pidx->bShowRes && (Type == fitImage)) {
			_tcscpy(p, GetDimensionStr());
			p += _tcslen(p);
			*p++ = '\n';
		}

		if (pidx->bShowSize) {
			_tcscpy(p, GetFileSizeStr());
			if (*p || (Type == fitImage)) {
				p += _tcslen(p);
				*p++ = '\n';
			}
		}

		if (pidx->bShowDate) {
			_tcscpy(p, GetFileTimeStr());
			p += _tcslen(p);
			*p++ = '\n';
		}

		*p = 0;

		SetTextColor(hdc, RGB(92,92,92));
		UINT uTextFormat = DT_TOP | DT_WORD_ELLIPSIS | DT_NOPREFIX;
		if (Type != fitImage) uTextFormat |= DT_CENTER;
		pDraw->Text(hdc, sz, rcText, IFileIconDrawing::eFileInfo, uTextFormat);
	}
}


bool FileIcon::HitTest(muiCoord xyTest)
{
	if (!IconPlacement.bVisible)
		return false;

	// Test first against grid
	muiCoord xyPos(IconPlacement.xyPos);

	if ((xyTest.x < xyPos.x) || (xyTest.y < xyPos.y) ||	(xyTest.x >= (xyPos.x+IconPlacement.whMaxIcon.w+2)) || (xyTest.y >= (xyPos.y+IconPlacement.whSize.h)))
		return false;
	
	// Now test agains real thumb-nail area
	muiSize whIcon;
	bool bCenterIcon;
	GetIconWH(whIcon, bCenterIcon);

	xyPos.y += (IconPlacement.whMaxIcon.h-whIcon.h)/2;
	if ((xyTest.y < xyPos.y) || (xyTest.y >= (xyPos.y+whIcon.h+32)))
		return false;

	if (!bCenterIcon) {
		if (xyTest.x >= (xyPos.x+whIcon.w+2))
			return false;
//	} else {
//		xyPos.x += (IconPlacement.whMaxIcon.w-whIcon.w)/2;
//		if ((xyTest.x < xyPos.x) || (xyTest.x >= (xyPos.+whIcon.w+2)))
//			return false;
	}
	
	return true;
}


void FileIcon::GetIconWH(muiSize &whIcon, bool &bCenterIcon)
{
	if (ImageInfo.pimd) {

		// We have an image thumbnail icon
		whIcon = { int(ImageInfo.pimd->DispX()), int(ImageInfo.pimd->DispY()) };

		bCenterIcon = false;

	}
	else if (Type == fitImage) {

		// We must be in the process of reading the image, but can't display it yet
		if ((ImageInfo.iX <= IconPlacement.whMaxIcon.w) && (ImageInfo.iY <= IconPlacement.whMaxIcon.h)) {
			whIcon = { ImageInfo.iX, ImageInfo.iY };
		}
		else if (ImageInfo.iX*IconPlacement.whMaxIcon.h > IconPlacement.whMaxIcon.w*ImageInfo.iY) {
			whIcon.w = IconPlacement.whMaxIcon.w;
			whIcon.h = (whIcon.w * ImageInfo.iY) / ImageInfo.iX;
		}
		else {
			whIcon.h = IconPlacement.whMaxIcon.h;
			whIcon.w = (whIcon.h * ImageInfo.iX) / ImageInfo.iY;
		}

		bCenterIcon = false;

	}
	else if (Type == fitUndecided) {

		// We haven't decided on the file type yet
		whIcon = IconPlacement.whMaxIcon;

		bCenterIcon = false;

	}
	else {

		// We have a document or a directory, for which we'll use Windows icons
		/*static BYTE cSystemIcoWH = 0;
		if (!cSystemIcoWH) cSystemIcoWH = GetSystemMetrics(SM_CYICON);

		whIcon = { cSystemIcoWH, cSystemIcoWH };*/
		whIcon = { 32, 32 };

		bCenterIcon = true;
	}
}


void FileIcon::DrawLongText(HDC hdc, IFileIconDrawing *pDraw, RECT &rc, UINT uTextFormat, IFileIconDrawing::eTextColor clr, PCTCHAR pcszText, int nTextLen, int &nMaxLines, bool bAddLinkSymbol) const
{
	const int nMaxTextLen = 500;
	TCHAR sz[nMaxTextLen+10+1];	// Max text length is nMaxTextLen, plus up to 10 inserted line-breaks, plus 1 terminating zero
	if (nTextLen > nMaxTextLen) nTextLen = nMaxTextLen;

	memcpy(sz, pcszText, nTextLen*sizeof(TCHAR));
	sz[nTextLen] = 0;

	int nMaxWidth = (rc.right - rc.left);
	int nLinesLeft = nMaxLines;

	PTCHAR p = sz, p2;

	while (nLinesLeft-- > 1) {

		// Cut away letters until it'll fit in the width available
		int nFullTextLen = nTextLen;
		while (nTextLen > 1) {
			if (pDraw->GetLengthOfText(hdc, p, nTextLen) < nMaxWidth) break;
			nTextLen--;
		}
		if (nTextLen == nFullTextLen) break;	// All fit!

		// Search for separators to break at
		for (p2 = p+nTextLen-1; p2 >= p + 4; p2--) {
			TCHAR c = *p2;
			TCHAR c2 = p2[-1];
			if (	(BYTE(c) <= BYTE(' ')) || strchr(".,;_-@\"", c) ||			// Separator chars
					((c >= '0') && (c <= '9') && ((c2 < '0') || (c2 > '9'))) ||	// char followed by number
					((c2 >= '0') && (c2 <= '9') && ((c < '0') || (c > '9'))) ||	// number followed by char
					((c >= 'A') && (c <= 'Z') && (c2 >= 'a') && (c2 <= 'z'))	// lower case followed by upper case
				) {
				nTextLen = p2 - p;
				break;
			}
		}

		// Insert line break
		p += nTextLen;
		nTextLen = nFullTextLen-nTextLen;
		if (WORD(*p) <= WORD(' ')) {
			nTextLen--;
		} else {
			iem_Move(p + 1, p, (nTextLen + 1)*sizeof(TCHAR));
		}
		*p++ = '\n';

		// Skip any blank chars at beginning of new line
		for (p2 = p; *p2 && (BYTE(*p2) <= BYTE(' ')); p2++) {
		}
		int nSkip = p2 - p;
		if (nSkip) {
			nTextLen -= nSkip;
			iem_Move(p, p + nSkip, nTextLen*sizeof(TCHAR));
		}
		p[nTextLen] = 0;
	}

	if (bAddLinkSymbol) {
		nTextLen = _tcslen(sz);
		sz[nTextLen++] = ' ';
#ifdef UNICODE
		sz[nTextLen++] = 0x2192;
#else
		sz[nTextLen++] = '-';
		sz[nTextLen++] = '-';
		sz[nTextLen++] = '>';
#endif
		sz[nTextLen] = 0;
	}

	if (Type != fitImage) uTextFormat |= DT_CENTER;

	pDraw->Text(hdc, sz, rc, clr, uTextFormat);

	nMaxLines = nLinesLeft;
}


void FileIcon::ShellOpenFile(HWND hwnd) const
{
	PCTCHAR pcszFile = FileInfo.File.Str();
	TCHAR szBuf[MAX_PATH];

	if (IsFileExt(_T(".lnk")))
		if (mdResolveShellLink(szBuf, pcszFile, true))
			pcszFile = szBuf;

	switch (Type) {

    case fitImage:
		SpawnViewer(pcszFile);
		break;

    case fitDirectory:
		// Handled in Index.cpp instead...
		break;

    default:
    	ShellExecute(hwnd, _T("open"), pcszFile, NULL, NULL, SW_SHOWNORMAL);
    	break;
    }
}
