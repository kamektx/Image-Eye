//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	WinMain + Shared functions + globals
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
#include <shlobj.h>

#include "ieColor.h"
#include "ieTextConv.h"
#include "iePreview.h"
#include "mdSystem.h"
#include "mdDlgCtrls.h"
#include "mdFileDlg.h"
#include "muiMain.h"
#include "../Res/Resource.h"


//------------------------------------------------------------------------------

#ifdef __X64__
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif


//------------------------------------------------------------------------------
// Globals
//------------------------------------------------------------------------------

ieFileManager	g_ieFM;							// Application global image File Manager
HINSTANCE		g_hInst;						// Application global process instance handle
iePProgress		g_pProgress = nullptr;			// Application global file-load progress (sometimes nullptr, sometimes non-nullptr but hidden - when pre-caching or previewing files - it still needs to be available in case we want to show the file being processed)


bool g_bLoadNewDontSpawnIndexHack = false;
bool g_bLoadNewSpawnIndexLaterHack = false;


//------------------------------------------------------------------------------

void OpenHelp(HWND hwnd)
{
	mdLaunchHtmlHelp(hwnd, pCfgInst->szExePath, pCfgInst->nExeNameOffs, _T(PROGRAMNAME ".chm"), HH_DISPLAY_TOC);
}


//------------------------------------------------------------------------------
// Progress reporting in the Windows tasbar
//------------------------------------------------------------------------------

class ProgressBox : public ieProgress {

public:
	ProgressBox(HWND hwndParent)
		: ieProgress(),
		pTBP(mdCreateTaskBarProgress(hwndParent)),
		hwnd(hwndParent),
		iPercent(0), bVisible(false)
	{
	}

	virtual ~ProgressBox()
	{
		if (!pTBP) return;
		if (bVisible) pTBP->SetState(mdITaskBarProgress::eNoProgress);
		delete pTBP;
	}

	virtual void Progress(int iDone, int iMax = 100)
	{
		if (iMax <= 0) iMax = 100;
		int iNewPercent = (((iDone * 100) / iMax) >> 1) << 1;
		if (iNewPercent == iPercent) return;
		iPercent = iNewPercent;

		if (!pTBP || !bVisible) return;

		pTBP->SetProgress(iPercent, 100);
	}

	virtual void Show()
	{
		if (!pTBP || bVisible) return;
		pTBP->SetState(mdITaskBarProgress::eNormal);
		pTBP->SetProgress(iPercent, 100);
		bVisible = true;
	}

	virtual void Hide()
	{
		if (!pTBP || !bVisible) return;
		pTBP->SetState(mdITaskBarProgress::eNoProgress);
		bVisible = false;
	}

private:
	mdITaskBarProgress *pTBP;
	HWND	hwnd;
	int		iPercent;
	bool	bVisible;
};


iePProgress g_CreateProgress(HWND hwndParent)
{
	return new ProgressBox(hwndParent);
}


//-----------------------------------------------------------------------------

class ieFileOpenDlg : public mdFileDlg {
public:
	ieFileOpenDlg();
	virtual ~ieFileOpenDlg();

	virtual void OnInitDialog(HWND hwnd);
	virtual void OnCommand(UINT idCtrl, WORD wEventCode, LPARAM lParam);

protected:
	virtual bool EnumFileFilters(int &n, PTCHAR pszzFilter);
	virtual bool OnGetFileInfo(PTCHAR pszInfo, DWORD nMaxInfoChars, PCTCHAR pcszFile);
	virtual void OnInitDone();
	virtual void OnResized();
	virtual bool OnDrawItem(DRAWITEMSTRUCT *pdi, UINT idCtrl);
	virtual void OnTimer(UINT idTimer);
	virtual void OnFileChanged(PCTCHAR pcszNewFile);

	UINT idPreview;
	iePreview Preview;				// Preview decoding handler
	HWND hwndPreview;
};


ieFileOpenDlg::ieFileOpenDlg()
	: mdFileDlg(false, IEYEICO),
	Preview(g_ieFM, NULL, g_pProgress), hwndPreview(nullptr)
{
	idTemplate = FILEOPENDLG;
	idRecent = OF_RECENT;
	idInfo = OF_INFO;
	idPreview = OF_PREVIEW;

	pRecent = &pCfgInst->RecentPaths;
	bAddAllFormats = false;
	Preview.SetBorder(iePreview::eBlackImageBorder);
}


ieFileOpenDlg::~ieFileOpenDlg()
{
}


void ieFileOpenDlg::OnInitDialog(HWND hwnd)
{
	hwndPreview = GetDlgItem(hwnd, OF_PREVIEW);

	mdFileDlg::OnInitDialog(hwnd);

	ieTranslateCtrls(hwnd);

	// Change 'Open' button to 'View'
	CtlButn::SetText(hwndDlg, IDOK, ieTranslate(_T("View")));

	// Translate 'Cancel' button
	CtlButn::SetText(hwndDlg, IDCANCEL, ieTranslate(_T("Cancel")));
}


void ieFileOpenDlg::OnInitDone()
{
	mdFileDlg::OnInitDone();

	// Move buttons
	OnResized();

	// Move to foreground!
	SetForegroundWindow(hwndDlg);

	// Start preview timer
	SetTimer(hwnd, 200, 100, nullptr);
}


void ieFileOpenDlg::OnTimer(UINT idTimer)
{
	// Select last opened file in the file list-view (this must be done delayed so that the list view have time to initialize the list...) 
	if ((idTimer < 200) || (idTimer >= 210)) return;
	KillTimer(hwnd, idTimer);

	for (;;) {
		// Retrieve handle list view and set it in focus
		HWND hcb = GetDlgItem(hwndDlg, 0x0461);
		if (!hcb) break;

		hcb = GetDlgItem(hcb, 1);
		if (!hcb) break;

		TCHAR sz[MAX_PATH];
		GetClassName(hcb, sz, sizeof(sz));
		if (_tcsicmp(sz, _T("SysListView32"))) return;

		SetFocus(hcb);

		// Retrieve last file name
		PCTCHAR pcszLastFile = pCfgInst->GetLastFile();
		if (!pcszLastFile || !*pcszLastFile) return;

		PCTCHAR pcszName, pcszExt;
		ief_SplitPath(pcszLastFile, pcszName, pcszExt);

		// Check if we're still in the same diretory
		*sz = 0;
		CommDlg_OpenSave_GetFolderPath(hwndDlg, sz, sizeof(sz));
		int nPathLen = _tcslen(sz);
		if (!nPathLen || (nPathLen != ((pcszName - 1) - pcszLastFile))) return;
		if (memcmp(pcszLastFile, sz, nPathLen*sizeof(TCHAR))) return;

		// Select the file
		LVFINDINFO	lvfi;
		lvfi.flags = LVFI_STRING;
		lvfi.psz = pcszName;
		int i = ListView_FindItem(hcb, -1, &lvfi);
		if (i < 0) break;

		ListView_EnsureVisible(hcb, i, FALSE);
		ListView_SetItemState(hcb, i, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);

		return; // return on Sucess!
	}

	// Wait a bit more and try again... (we'll do max 10*100ms)
	SetTimer(hwnd, idTimer + 1, 100, nullptr);
}


void ieFileOpenDlg::OnResized()
{
	mdFileDlg::OnResized();

	// Reposition our Index and Help buttons
	HWND hwndHelpBtn = GetDlgItem(hwnd, OF_HELP);
	HWND hwndIndexBtn = GetDlgItem(hwnd, OF_INDEX);
	HWND hwndPreview = GetDlgItem(hwnd, OF_PREVIEW);
	HWND hwndOkBtn = GetDlgItem(hwndDlg, IDOK);
	HWND hwndCancelBtn = GetDlgItem(hwndDlg, IDCANCEL);

	if (hwndIndexBtn && hwndOkBtn && hwndCancelBtn) {

		RECT rcHelp, rcOK, rcCancel;
		if (GetWindowRect(hwndHelpBtn, &rcHelp) && GetWindowRect(hwndOkBtn, &rcOK) && GetWindowRect(hwndCancelBtn, &rcCancel)) {

			POINT xy, wh;
			int ystride = abs(rcCancel.bottom - rcOK.bottom);
			xy.x = rcCancel.left;
			xy.y = max(rcCancel.top, rcOK.top) + ystride;

			if ((xy.x != rcHelp.left) || (xy.y != rcHelp.top)) {

				wh.x = rcCancel.right - rcCancel.left;
				wh.y = rcCancel.bottom - rcCancel.top;

				if (ScreenToClient(hwnd, &xy)) {
					MoveWindow(hwndHelpBtn, xy.x, xy.y, wh.x, wh.y, TRUE);

					RECT rcPreview;
					if (hwndPreview && GetWindowRect(hwndPreview, &rcPreview)) {
						POINT xyBottom = { rcPreview.left, rcPreview.bottom };
						ScreenToClient(hwnd, &xyBottom);
						xy.y = ((xy.y + ystride) + (xyBottom.y - ystride)) / 2;
					}
					else {
						xy.y += ystride;
					}

					MoveWindow(hwndIndexBtn, xy.x, xy.y, wh.x, wh.y, TRUE);
				}
			}
		}
	}
}


bool ieFileOpenDlg::OnDrawItem(DRAWITEMSTRUCT *pdi, UINT idCtrl)
{
	if (mdFileDlg::OnDrawItem(pdi, idCtrl))
		return true;

	if (idCtrl == idPreview) {
		if ((pdi->itemAction & ODA_DRAWENTIRE) != 0) {
			Preview.DrawPreview(pdi->hDC);
			//ValidateRect(>hwndPreview, nullptr);
		}
		return true;
	}

	return false;
}


void ieFileOpenDlg::OnCommand(UINT idCtrl, WORD wEventCode, LPARAM lParam)
{
	mdFileDlg::OnCommand(idCtrl, wEventCode, lParam);

	switch (idCtrl) {

	case OF_PREVIEW:

		CtlButn::Click(hwndDlg, IDOK);

		break;

	case OF_INDEX: {

		TCHAR sz[MAX_PATH + 10];

		CommDlg_OpenSave_GetFolderPath(hwndDlg, sz, (sizeof(sz) / sizeof(TCHAR)));

		int i = _tcslen(sz);
		if ((sz[i - 1] != '\\') && (sz[i - 1] != '/')) {
			sz[i++] = '\\';
			sz[i] = 0;
		}

		CtlButn::Click(hwndDlg, IDCANCEL);

		if (g_bLoadNewDontSpawnIndexHack) {
			SetCurrentDirectory(sz);
			g_bLoadNewSpawnIndexLaterHack = true;
			break;
		}

		SpawnIndex(sz);

	}	break;

	case OF_HELP:
		OpenHelp(hwnd);
		break;

	}
}


bool ieFileOpenDlg::EnumFileFilters(int &n, PTCHAR pszzFilter)
{
	PCTCHAR pcszDesc, pcszExt;
	DWORD fTypes;

	if (n == 0) {

		_tcscpy(pszzFilter, ieTranslate(_T("Supported file types")));
		pszzFilter += _tcslen(pszzFilter) + 1;
		for (int i = 0; g_ieFM.EnumFileReaders(i, pcszExt, pcszDesc); i++) {
			_stprintf(pszzFilter, _T("*.%s;"), pcszExt);
			pszzFilter += _tcslen(pszzFilter);
		}
		_stprintf(pszzFilter, _T("*.ies%c"), 0);	// Add .IES slideshow files not added by EnumFileReaders
													//pszzFilter += _tcslen(pszzFilter);
													//pszzFilter[-1] = 0;
		return true;

	}
	else if (n == 1) {

		_tcscpy(pszzFilter, ieTranslate(_T("All files")));
		pszzFilter += _tcslen(pszzFilter) + 1;
		_tcscpy(pszzFilter, _T("*.*"));
		pszzFilter += _tcslen(pszzFilter);
		pszzFilter[1] = 0;
		return true;
	}

	int m = n - 2, m0 = m;
	if (!g_ieFM.EnumFileReaders(m, pcszExt, pcszDesc))
		return false;
	n += (m - m0);

	for (PCTCHAR pcsz = pcszExt;;) {
		TCHAR c = *pcsz++;
		if (!c) break;
		*pszzFilter++ = (TCHAR)_totupper(c);
	}

	*pszzFilter++ = ' ';
	*pszzFilter++ = '-';
	*pszzFilter++ = ' ';

	_tcscpy(pszzFilter, pcszDesc);
	pszzFilter += _tcslen(pszzFilter) + 1;

	*pszzFilter++ = '*';
	*pszzFilter++ = '.';
	_tcscpy(pszzFilter, pcszExt);
	pszzFilter += _tcslen(pszzFilter) + 1;

	*pszzFilter = 0;

	return true;
}


void ieFileOpenDlg::OnFileChanged(PCTCHAR pcszNewFile)
{
	Preview.LoadPreview(hwndPreview, nullptr);

	mdFileDlg::OnFileChanged(pcszNewFile);
}


bool ieFileOpenDlg::OnGetFileInfo(PTCHAR pszInfo, DWORD nMaxInfoChars, PCTCHAR pcszFile)
{
	iePFileReader pifr = nullptr;
	if (!ieIsOk(g_ieFM.CreateFileReader(pifr, pcszFile, false))) {
		if (pifr) pifr->Release();
		pifr = nullptr;
	}

	if (!pifr) {

		PCTCHAR pcszFileExt = nullptr;
		for (PCTCHAR pcsz = pcszFile; *pcsz;)
			if (*pcsz++ == '.')
				pcszFileExt = pcsz;

		if (pcszFileExt && ((*pcszFileExt == 'i') || (*pcszFileExt == 'I'))) {
			if (!_tcsicmp(pcszFileExt, _T("iei"))) {
				_stprintf(pszInfo, _T("%s index cache file"), _T(PROGRAMNAME));
			}
			else if (!_tcsicmp(pcszFileExt, _T("ies"))) {
				_stprintf(pszInfo, _T("%s slide show file"), _T(PROGRAMNAME));
			}
			else if (!_tcsicmp(pcszFileExt, _T("iea"))) {
				_stprintf(pszInfo, _T("%s image adjustment file"), _T(PROGRAMNAME));
			}
		}
	}

	if (pifr) {
		if (pifr->Have(ieTextInfoType::SourceFileFormat)) {
			PCTCHAR pcsz = pifr->Get(ieTextInfoType::SourceFileFormat);
			int nFormatLen = min(_tcslen(pcsz), nMaxInfoChars);
			memcpy(pszInfo, pcsz, nFormatLen*sizeof(TCHAR));
			pszInfo[nFormatLen] = 0;
			pszInfo += nFormatLen;
			nMaxInfoChars -= nFormatLen;
		}

		if (nMaxInfoChars >= 17) {
			_stprintf(pszInfo, _T("\n\n%dx%dx%d"), pifr->X(), pifr->Y(), pifr->Z());
			int nResLen = _tcslen(pszInfo);
			pszInfo += nResLen;
			nMaxInfoChars -= nResLen;
		}

		if (nMaxInfoChars >= 10) {
			PTCHAR pcExtraBeg = pszInfo;
			*pszInfo++ = '\n';
			*pszInfo++ = '\n';
			*pszInfo++ = '\n';
			*pszInfo = 0;
			nMaxInfoChars -= 3;
			int nExtraLen;
			if (pifr->Have(ieTextInfoType::Comment)) {
				PCTCHAR pcsz = pifr->Get(ieTextInfoType::Comment);
				nExtraLen = min(_tcslen(pcsz), nMaxInfoChars);
				memcpy(pszInfo, pcsz, nExtraLen*sizeof(TCHAR));
				pszInfo[nExtraLen] = 0;
			}
			else if (pifr->Have(ieTextInfoType::Author)) {
				PCTCHAR pcsz = pifr->Get(ieTextInfoType::Author);
				nExtraLen = min(_tcslen(pcsz), nMaxInfoChars);
				memcpy(pszInfo, pcsz, nExtraLen*sizeof(TCHAR));
				pszInfo[nExtraLen] = 0;
			}
			else if (pifr->Have(ieTextInfoType::Name)) {
				PCTCHAR pcsz = pifr->Get(ieTextInfoType::Name);
				nExtraLen = min(_tcslen(pcsz), nMaxInfoChars);
				memcpy(pszInfo, pcsz, nExtraLen*sizeof(TCHAR));
				pszInfo[nExtraLen] = 0;
			}
			if (nExtraLen > 30) {
				*pcExtraBeg = ' ';
			}
		}

		Preview.LoadPreview(hwndPreview, pifr);
	}


	return (pifr != nullptr);
}


bool ShowOpenFileDialog(HWND hwnd, PTCHAR pszFileName, int nMaxFileNameLen, bool bMultiple)
{
	// Init input fields
	TCHAR szInitPath[MAX_PATH];
	*szInitPath = 0;
	pCfgInst->RecentPaths.Get(szInitPath);

	TCHAR 	szDlgTitle[256];
	_stprintf(szDlgTitle, _T("%s | %s"), ieTranslate(_T("Open image file")), _T(PROGRAMVERNAME));

	// Ask for file name
	ieFileOpenDlg fod;

	if (!fod.GetFileName(hwnd, pszFileName, nMaxFileNameLen, szDlgTitle, szInitPath))
		return false;

	return true;
}


//-----------------------------------------------------------------------------

bool SaveImage(HWND hwnd, iePImageDisplay pimd, PCTCHAR pcszFile, bool bSaveOrig)
// Save the current image
{
	TCHAR szFile[MAX_PATH];
	if (!pcszFile) {

		// Ask for file name
		PCTCHAR pcszName, pcszExt;
		pcszName = pimd->Text()->Get(ieTextInfoType::SourceFile);
		if (!pcszName) pcszName = pimd->Text()->Get(ieTextInfoType::Name);
		if (!pcszName || !*pcszName) pcszName = _T("foobar");
		_tcscpy(szFile, pcszName);
		ief_SplitPath(szFile, pcszName, pcszExt);
		_tcscpy(szFile + (pcszExt - szFile), _T(".bmp"));

		mdFileDlg sfd(true, IEYEICO);
		sfd.SetFileFilters(_T("Windows bitmap (.bmp)\0*.bmp\0"));
		if (!sfd.GetFileName(hwnd, szFile, sizeof(szFile) / sizeof(TCHAR), ieTranslate(_T("Save image as")), szFile))
			return false;

		// Ensure we have an extension
		ief_SplitPath(szFile, pcszName, pcszExt);
		if (_tcsicmp(pcszExt, _T(".bmp"))) {
			_tcscpy(szFile + (pcszExt - szFile), _T(".bmp"));
		}

		// Save the last directory
		if (pcszName > szFile) {
			szFile[pcszName - szFile - 1] = 0;
			pCfgInst->RecentPaths.Set(szFile);
			szFile[pcszName - szFile - 1] = '\\';
		}

		pcszFile = szFile;
	}


	// Save as .bmp
	IE_HFILE hf = CreateFile(pcszFile, GENERIC_WRITE | GENERIC_READ, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (hf == IE_INVALIDHFILE) return false;

	iePImage pim = bSaveOrig ? pimd->OrigImage() : pimd->DispImage();

	PBYTE pbImage;
	int iPixelSize, iPitch;
	iePBGRA pCLUT;
	if (pim->CLUT()) {
		iPixelSize = 1;
		iPitch = pim->CLUT()->Pitch();
		pbImage = PBYTE(pim->CLUT()->PixelPtr());
		pCLUT = pim->CLUT()->CLUTPtr();
	}
	else { //if (pimOrig->BGRA()) {
		iPixelSize = 4;
		iPitch = pim->BGRA()->Pitch();
		pbImage = PBYTE(pim->BGRA()->PixelPtr());
		pCLUT = nullptr;
	}

	int cbSrcPitch = iPitch*iPixelSize;
	int cbDstPitch = (pim->X()*(pCLUT ? 1 : 3) + 3) & (~3);
	PBYTE pbSrc = pbImage + pim->Y()*cbSrcPitch;
	PBYTE pbBuf = nullptr;
	if (!pCLUT) pbBuf = new BYTE[cbDstPitch + 16];

	WORD bfType;

	struct {
		DWORD	bfSize;
		WORD	bfRes1;
		WORD	bfRes2;
		DWORD	bfOffset;
	} bmfh;

	struct {
		DWORD	biSize;
		DWORD	biWidth;
		SDWORD	biHeight;
		WORD	biPlanes;
		WORD	biBitCount;
		DWORD	biCompression;
		DWORD	biSizeImage;
		DWORD	biXPelsPerMeter;
		DWORD	biYPelsPerMeter;
		DWORD	biClrUsed;
		DWORD	biClrImportant;
	} bmih;

	bfType = 0x4D42;

	bmfh.bfOffset = sizeof(bfType) + sizeof(bmfh) + sizeof(bmih);
	if (pCLUT) bmfh.bfOffset += 256 * 4;
	bmfh.bfRes1 = bmfh.bfRes2 = 0;
	bmfh.bfSize = bmfh.bfOffset + pim->Y()*cbDstPitch;

	bmih.biSize = sizeof(bmih);
	bmih.biWidth = pim->X();
	bmih.biHeight = pim->Y();
	bmih.biPlanes = 1;
	bmih.biBitCount = pCLUT ? 8 : 24;
	bmih.biCompression = BI_RGB;
	bmih.biSizeImage = 0;
	iePixelDensity pd = { 0, 0 };
	ie_ParsePixelDensityInfoStr(pd, pim->Text()->Get(ieTextInfoType::PixelDensity), ieDotsPerMeter);
	bmih.biXPelsPerMeter = pd.dwXpU;
	bmih.biYPelsPerMeter = pd.dwYpU;
	bmih.biClrUsed = bmih.biClrImportant = 0;

	ief_Write(hf, &bfType, sizeof(bfType));
	ief_Write(hf, &bmfh, sizeof(bmfh));
	ief_Write(hf, &bmih, sizeof(bmih));
	if (pCLUT) {
		RGBQUAD rgbq[256];
		for (int i = 0; i < 256; i++) {
			rgbq[i].rgbBlue = pCLUT[i].B;
			rgbq[i].rgbGreen = pCLUT[i].G;
			rgbq[i].rgbRed = pCLUT[i].R;
			rgbq[i].rgbReserved = 0;
		}
		ief_Write(hf, &rgbq, 256 * sizeof(RGBQUAD));
	}


	for (int yc = pim->Y(), xw = pim->X(); yc--; ) {
		pbSrc -= cbSrcPitch;

		if (pCLUT) {
			ief_Write(hf, pbSrc, cbDstPitch);
		}
		else {
			ie_BGRA2BGR(iePBGR(pbBuf), iePBGRA(pbSrc), xw);
			ief_Write(hf, pbBuf, cbDstPitch);
		}
	}

	if (pbBuf) delete[] pbBuf;

	CloseHandle(hf);

	return true;
}


//-----------------------------------------------------------------------------

bool ie_DeleteFileQuery(HWND hwnd, PCTCHAR pcszFile, int nNumFiles)
{
	if ((!*pcszFile) || (*pcszFile == '<')) return false;

	TCHAR szBuff[2 * MAX_PATH + 64];
	if (nNumFiles > 1) {
		_stprintf(szBuff, (nNumFiles == 2) ? _T("%s\n\n%s") : _T("%s\n\n%s\n\n... <+%d> ..."), pcszFile, pcszFile + _tcslen(pcszFile) + 1, nNumFiles - 2);
		pcszFile = szBuff;
	}

	return (IDYES == MessageBox(hwnd, pcszFile, ieTranslate(_T("Do you really want to delete this?")), MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2 | MB_APPLMODAL | MB_TOPMOST));
}


enum ieFileOpType { ieDeleteFile = 3, ieRenameFile = 4, ieMoveFile = 1, ieCopyFile = 2 };
// File operations that can be done in cooperation with the cache (i.e. without having the re-create the cache)


static bool ie_DoFileOp(HWND hwnd, ieFileOpType eOp, PCTCHAR pcszSrc, PCTCHAR pcszDst = nullptr, int nNumFiles = 1)
// Do file operation (delete/move/rename/copy), pcszSrc is zero terminated if nNumFiles == 1, double-zero terminated list of sz's if nNumFiles > 1
{
	// Validate options
	if (!pcszSrc || !*pcszSrc) return false;
	switch (eOp) {
	case ieDeleteFile:
		pcszDst = nullptr;
		break;
	case ieRenameFile:
	case ieMoveFile:
	case ieCopyFile:
		if (!pcszDst || !*pcszDst) return false;
		break;
	default:
		return false;
	}

	// Figure out path lengths
	int nSrcLen, nDstLen, nSrcPathLen, nDstPathLen;

	nSrcPathLen = nSrcLen = int(_tcslen(pcszSrc));
	while (nSrcPathLen--)
		if (pcszSrc[nSrcPathLen] == '\\')
			break;
	nSrcPathLen++;

	if (pcszDst) {
		nDstPathLen = nDstLen = int(_tcslen(pcszDst));
		if ((eOp == ieRenameFile) || !ief_IsDir(pcszDst)) {
			while (nDstPathLen--)
				if (pcszDst[nDstPathLen] == '\\')
					break;
			nDstPathLen++;
		}
	}

	// Verify paths for eOp
	switch (eOp) {
	case ieRenameFile:
		if (nDstPathLen) {
			if ((nSrcPathLen != nDstPathLen) || _tcsnicmp(pcszSrc, pcszDst, nDstPathLen)) return false;
		}
		break;
	case ieMoveFile:
		if ((nSrcPathLen == nDstPathLen) && !_tcsnicmp(pcszSrc, pcszDst, nDstPathLen)) {
			eOp = ieRenameFile;
		}
		break;
	case ieCopyFile:
		if (!nDstPathLen) return false;
		if ((nSrcPathLen == nDstPathLen) && !_tcsnicmp(pcszSrc, pcszDst, nDstPathLen)) return false;
		break;
	}

	// Construct double-zero terminated src & dst strings + figure out path lengths
	TCHAR szzSrcBuf[MAX_PATH + 1], szzDstBuf[MAX_PATH + 1];
	PCTCHAR pcszzSrc, pcszzDst;
	if (nNumFiles == 1) {
		memcpy(szzSrcBuf, pcszSrc, (nSrcLen + 1)*sizeof(*pcszSrc));
		szzSrcBuf[nSrcLen + 1] = 0;
		pcszzSrc = szzSrcBuf;
	}
	else {
		pcszzSrc = pcszSrc;
	}
	if (pcszDst) {
		memcpy(szzDstBuf, pcszDst, (nDstLen + 1)*sizeof(*pcszDst));
		szzDstBuf[nDstLen + 1] = 0;
		pcszzDst = szzDstBuf;
	}
	else {
		pcszzDst = nullptr;
	}

#ifdef IE_SUPPORT_DIRCACHE
	// Pause monitoring for changes
	ieIDirectoryEnumerator *pEnumSrc = nullptr;
	if (nSrcPathLen && (eOp != ieCopyFile)) {
		TCHAR szPath[IE_FILENAMELEN];
		memcpy(szPath, pcszSrc, nSrcPathLen*sizeof(TCHAR));
		szPath[nSrcPathLen] = 0;
		pEnumSrc = g_ieFM.DirCache.CreateEnumerator(szPath, true);
		if (pEnumSrc) {
			pEnumSrc->PauseMonitoring(true);
		}
	}
#endif

	// Do file op using Shell-API (allowing recycle bin, undo, progress, ...)
	SHFILEOPSTRUCT	fo;
	iem_Zero(&fo, sizeof(fo));
	fo.hwnd = hwnd;
	fo.wFunc = UINT(eOp);
	fo.pFrom = pcszzSrc;
	fo.pTo = pcszzDst;
	fo.fFlags = FOF_ALLOWUNDO;
	if (eOp == ieDeleteFile) fo.fFlags |= FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI;

	int ires = SHFileOperation(&fo);
	bool bFileOpDone = (ires == 0);

#ifdef IE_SUPPORT_DIRCACHE
	// Modify directory cache data & unpause monitoring
	if (pEnumSrc) {
		if (bFileOpDone) for (;;) {
			pEnumSrc->ModifyFileName(pcszSrc + nSrcPathLen, ((eOp == ieRenameFile) && pcszzDst) ? (pcszzDst + nDstPathLen) : nullptr);
			if (--nNumFiles <= 0) break;
			pcszSrc += (nSrcLen + 1);
		}
		pEnumSrc->PauseMonitoring(false);
		pEnumSrc->Release();
	}
#endif
	// Return result
	return bFileOpDone || fo.fAnyOperationsAborted;
}


bool ie_DeleteFile(HWND hwnd, PCTCHAR pcszFile, bool bEraseFile, int nNumFiles)
{
	if (!pcszFile || !*pcszFile || (nNumFiles < 1)) return false;

#ifdef IE_SUPPORT_IMAGECACHE
	g_ieFM.ImageCache.FlushFile(pcszFile);
#endif

	bool r = true;

	if (!bEraseFile) {

		// Normal delete - call ieDirectoryCache, which will call SHFileOperation(...)
		bool r = ie_DoFileOp(hwnd, ieDeleteFile, pcszFile, nullptr, nNumFiles);
		if (!r) r = ief_Delete(pcszFile);

		if (r) {
			for (int n = nNumFiles; n--; pcszFile += _tcslen(pcszFile) + 1) {

				TCHAR szIEA[MAX_PATH + 4];	 // /0/0 terminated list of files
				_stprintf(szIEA, _T("%s.iea"), pcszFile);

				if (ief_Exists(szIEA)) {
					ief_Delete(szIEA);
				}
			}
		}



	}
	else for (int n = nNumFiles; r && n--; pcszFile += _tcslen(pcszFile) + 1) {

		// Erase (wipe) files

		for (int m = 2; r && m--; ) {

			TCHAR szFrom[MAX_PATH + 4];
			if (m) {
				_stprintf(szFrom, _T("%s.iea"), pcszFile);
				if (!ief_Exists(szFrom)) continue;
			}
			else {
				_tcscpy(szFrom, pcszFile);
			}

			// Overwrite with random data
			SetFileAttributes(szFrom, FILE_ATTRIBUTE_NORMAL);
			IE_HFILE hf;

			for (int i = 0; i < 3; i++) {
				hf = CreateFile(szFrom, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
				if (hf == IE_INVALIDHFILE) continue;

				BYTE acRandom[4096];
				for (int j = 0; j < 4096; j++)
					acRandom[j] = BYTE(rand());

				for (DWORD dwLeft = ief_Size(hf); dwLeft > 0; ) {
					DWORD dwThis = min(dwLeft, sizeof(acRandom));

					ief_Write(hf, acRandom, dwThis);

					dwLeft -= dwThis;
				}

				FlushFileBuffers(hf);

				CloseHandle(hf);
			}

			// Change name a couple of times
			TCHAR szNew[MAX_PATH];
			_tcscpy(szNew, szFrom);
			int iStrLen = _tcslen(szNew);
			PTCHAR pszName = szNew + iStrLen;
			for (; (pszName >= szNew) && (*pszName != '\\') && (*pszName != '/'); pszName--) {
			}
			pszName++;
			int iPathLen = pszName - szNew;
			int iNameLen = iStrLen - iPathLen;

			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < iNameLen; j++) {
					if (pszName[j] == '.') continue;
					pszName[j] = 'a' + (rand() % 25);
				}
				MoveFile(szFrom, szNew);
				_tcscpy(szFrom, szNew);
			}

			// Make it 1-Byte long
			hf = CreateFile(szFrom, GENERIC_WRITE, 0, nullptr, TRUNCATE_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH, nullptr);
			if (hf != INVALID_HANDLE_VALUE) {
				int i = 0;
				ief_Write(hf, &i, 1);
				FlushFileBuffers(hf);
				CloseHandle(hf);
			}

			// Now, delete it!
			r = (DeleteFile(szFrom) != 0);
		}
	}

	if (!r) {
		MessageBox(hwnd, ieTranslate(_T("File operation failed!")), _T(PROGRAMNAME), MB_OK);
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------

static INT_PTR CALLBACK RenameFileDlg(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static PTCHAR pszName;
	switch (uMsg) {

	case WM_INITDIALOG:
		ieTranslateCtrls(hwnd);
		pszName = (PTCHAR)lParam;
		CtlEdit::SetText(hwnd, FRN_SRC, (PTCHAR)pszName);
		CtlEdit::SetText(hwnd, FRN_DST, (PTCHAR)pszName);
		SetFocus(GetDlgItem(hwnd, FRN_DST));
		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case IDOK:
			CtlEdit::GetText(hwnd, FRN_DST, pszName, MAX_PATH);
			EndDialog(hwnd, 1);
			break;

		case IDCANCEL:
			EndDialog(hwnd, 0);
			break;
		}
		return 1;
	}

	return FALSE;
}


bool ie_RenameFile(HWND hwnd, PTCHAR pszFile)
{
	// Parse out name part
	TCHAR szNewName[MAX_PATH], szEditName[MAX_PATH];
	_tcscpy(szNewName, pszFile);

	PCTCHAR pcszName, pcszExt;
	ief_SplitPath(szNewName, pcszName, pcszExt);

	int iNameLen = (pcszExt - pcszName);
	memcpy(szEditName, pcszName, iNameLen*sizeof(TCHAR));
	szEditName[iNameLen] = 0;

	// Let user edit it
	if (!DialogBoxParam(g_hInst, MAKEINTRESOURCE(FILERENAMEDLG), hwnd, &RenameFileDlg, (LPARAM)szEditName))
		return false;

	// Construct new fully qualified path & name
	_tcscat(szEditName, pcszExt);
	_tcscpy(szNewName + (pcszName - szNewName), szEditName);

	// Rename it!
	bool r = ie_DoFileOp(hwnd, ieRenameFile, pszFile, szNewName);
	if (!r) {
		MessageBox(hwnd, ieTranslate(_T("File operation failed!")), _T(PROGRAMNAME), MB_OK);
		return false;
	}

	// Any associated .iea file to rename too?
	_tcscat(pszFile, _T(".iea"));
	if (ief_Exists(pszFile)) {
		int n = _tcslen(szNewName);
		_tcscpy(szNewName + n, _T(".iea"));
		r = ie_DoFileOp(hwnd, ieRenameFile, pszFile, szNewName);
		szNewName[n] = 0;
	}

	// Store new file name
	_tcscpy(pszFile, szNewName);
	return true;
}


//-----------------------------------------------------------------------------

static int CALLBACK BrowseForFolderCallback(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	if (uMsg == BFFM_INITIALIZED) {
		if (lpData) SendMessage(hwnd, BFFM_SETSELECTION, TRUE, lpData);		// Set initial directory
	}
	return 0;
}

static bool BrowseForFolder(HWND hwndParent, PCTCHAR pcszTitle, PTCHAR pszFolder, bool bInitFolder)
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	BROWSEINFO bi;
	iem_Zero(&bi, sizeof(bi));
	bi.hwndOwner = hwndParent;
	bi.lpszTitle = pcszTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS;	// Was using | BIF_NEWDIALOGSTYLE; but that is not allowed in combination with COINIT_MULTITHREADED!
	bi.lpfn = BrowseForFolderCallback;
	bi.lParam = bInitFolder ? LPARAM(pszFolder) : NULL;

	PIDLIST_ABSOLUTE pidl = SHBrowseForFolder(&bi);
	if (pidl) {

		SHGetPathFromIDList(pidl, pszFolder);

		IMalloc *pMalloc;
		SHGetMalloc(&pMalloc);
		pMalloc->Free(pidl);
		pMalloc->Release();
	}

	CoUninitialize();

	return (pidl != nullptr);
}

//-----------------------------------------------------------------------------

bool ie_CopyFile(HWND hwnd, PCTCHAR pcszFile, bool bMoveFile, int nNumFiles)
{
	if (!pcszFile || !*pcszFile) return false;

	static PTCHAR pszDst = nullptr;
	if (!pszDst) {
		pszDst = new TCHAR[MAX_PATH];
		if (!pszDst) return false;
		*pszDst = 0;
		pCfgInst->RecentPaths.Get(pszDst);
	}

	if (!BrowseForFolder(hwnd, ieTranslate(_T("Select destination folder...")), pszDst, true))
		return false;

	bool r = ie_DoFileOp(hwnd, bMoveFile ? ieMoveFile : ieCopyFile, pcszFile, pszDst, nNumFiles);

	if (r) pCfgInst->RecentPaths.Set(pszDst);

	return r;
}


bool ie_CopyFile(HWND hwnd, PCTCHAR pcszDstFile, PCTCHAR pcszSrcFile)
{
	return ie_DoFileOp(hwnd, ieCopyFile, pcszSrcFile, pcszDstFile);
}


//-----------------------------------------------------------------------------

static BOOL CALLBACK EnumWindowsCloseAll(HWND hwnd, LPARAM lParam)
{
	TCHAR sz[512];
	GetWindowText(hwnd, sz, sizeof(sz) / sizeof(TCHAR));

	if (_tcsstr(sz, _T(PROGRAMNAME))) {
		SendMessage(hwnd, WM_COMMAND, CMD_EXIT, 0);
	}

	return TRUE;
}


void CloseAllImageEyeWindows()
{
	EnumWindows((WNDENUMPROC)EnumWindowsCloseAll, 0);
}


//-----------------------------------------------------------------------------

static BOOL CALLBACK EnumWindowsUpdateAll(HWND hwnd, LPARAM lParam)
{
	UpdateWindow(hwnd);
	return TRUE;
}


typedef struct WindowList {
	HWND hwnd;
	struct WindowList *pPrev;
	struct WindowList *pNext;
} WindowList;


static void InsertInWindowList(WindowList *pwlNew, PCTCHAR pcszNew, WindowList *pwl)
{
	TCHAR sz[512];
	GetWindowText(pwl->hwnd, sz, sizeof(sz) / sizeof(TCHAR));

	if (_tcsicmp(pcszNew, sz) < 0) {
		if (pwl->pPrev) {
			GetWindowText(pwl->pPrev->hwnd, sz, sizeof(sz) / sizeof(TCHAR));
			if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, pcszNew, -1, sz, -1) == CSTR_LESS_THAN) {
				InsertInWindowList(pwlNew, pcszNew, pwl->pPrev);
				return;
			}
			pwl->pPrev->pNext = pwlNew;
		}
		pwlNew->pPrev = pwl->pPrev;
		pwlNew->pNext = pwl;
		pwl->pPrev = pwlNew;
	}
	else {
		if (pwl->pNext) {
			GetWindowText(pwl->pNext->hwnd, sz, sizeof(sz) / sizeof(TCHAR));
			if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, pcszNew, -1, sz, -1) == CSTR_GREATER_THAN) {
				InsertInWindowList(pwlNew, pcszNew, pwl->pNext);
				return;
			}
			pwl->pNext->pPrev = pwlNew;
		}
		pwlNew->pNext = pwl->pNext;
		pwlNew->pPrev = pwl;
		pwl->pNext = pwlNew;
	}
}


static BOOL CALLBACK EnumMakeWindowList(HWND hwnd, LPARAM lParam)
{
	TCHAR sz[512];
	GetWindowText(hwnd, sz, sizeof(sz) / sizeof(TCHAR));

	if (_tcsstr(sz, _T(PROGRAMNAME))) {

		WindowList *pwl = (WindowList *)lParam;

		if (!pwl->hwnd) {	// First entry
			pwl->hwnd = hwnd;
		}
		else {
			WindowList *pwlNew = new WindowList;
			pwlNew->hwnd = hwnd;
			InsertInWindowList(pwlNew, sz, pwl);
		}
	}

	return TRUE;
}


void NextPrevWindow(HWND hwndThis, bool bPrev)
{
	WindowList *pwlHead = new WindowList;				// Start with a dummy
	pwlHead->hwnd = nullptr;
	pwlHead->pPrev = nullptr;
	pwlHead->pNext = nullptr;

	EnumWindows((WNDENUMPROC)EnumMakeWindowList, (LPARAM)pwlHead);
	if (!pwlHead->hwnd) pwlHead->hwnd = hwndThis;		// Shouldn't happen...

	while (pwlHead->pPrev) pwlHead = pwlHead->pPrev;	// Find true head

	WindowList *pwlThis = pwlHead;						// Find hwndThis
	while (pwlThis && (pwlThis->hwnd != hwndThis)) pwlThis = pwlThis->pNext;
	if (!pwlThis) pwlThis = pwlHead;					// Shouldn't happen...

	WindowList *pwlSelect = pwlThis;					// Find next or previous window
	if (bPrev) {
		if (pwlSelect->pPrev) pwlSelect = pwlSelect->pPrev;
		else while (pwlSelect->pNext) pwlSelect = pwlSelect->pNext;
	}
	else {
		if (pwlSelect->pNext) pwlSelect = pwlSelect->pNext;
		else pwlSelect = pwlHead;
	}
	HWND hwnd = pwlSelect->hwnd;

	for (; pwlHead; pwlHead = pwlThis) {				// Delete list
		pwlThis = pwlHead->pNext;
		delete pwlHead;
	}

	if (hwnd) SetForegroundWindow(hwnd);				// Set new foreground window!
}


//-----------------------------------------------------------------------------

bool CaptureWindow(iePImageDisplay &pimd, bool bFullScreen)
{
	if (bFullScreen) EnumWindows((WNDENUMPROC)EnumWindowsUpdateAll, 0);
	Sleep(bFullScreen ? 250 : 2000);
	HWND hwnd = GetForegroundWindow();
	if (!bFullScreen) UpdateWindow(hwnd);

	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	bool bWin8OrLater = GetVersionEx(&osvi) && (osvi.dwMajorVersion * 100 + osvi.dwMinorVersion >= 602);

	HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
	MONITORINFOEX mi;
	mi.cbSize = sizeof(mi);
	if (!hMon || !GetMonitorInfo(hMon, &mi)) return false;
	HDC hdc = CreateDC(NULL, mi.szDevice, NULL, NULL);
	if (!hdc) return false;

	ieXY xyPos;
	ieWH whSize;

	if (bFullScreen) {
		xyPos.nX = mi.rcMonitor.left;
		xyPos.nY = mi.rcMonitor.top;
		whSize.nX = mi.rcMonitor.right - mi.rcMonitor.left;
		whSize.nY = mi.rcMonitor.bottom - mi.rcMonitor.top;
	}
	else {
		RECT rc;
		if (!GetWindowRect(hwnd, &rc)) return false;
		if (bWin8OrLater) {
			static HMODULE hDWM = NULL;
			if (!hDWM) hDWM = LoadLibraryA("dwmapi.dll");
			if (hDWM) {
				typedef HRESULT __stdcall TDwmGetWindowAttribute(HWND hwnd, DWORD dwAttribute, void *pvAttribute, DWORD cbAttribute);
#define DWMWA_EXTENDED_FRAME_BOUNDS	9
				static TDwmGetWindowAttribute * pDwmGetWindowAttribute = nullptr;
				if (!pDwmGetWindowAttribute) pDwmGetWindowAttribute = (TDwmGetWindowAttribute *)GetProcAddress(hDWM, "DwmGetWindowAttribute");
				if (pDwmGetWindowAttribute) {
					HRESULT hr = (*pDwmGetWindowAttribute)(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &rc, sizeof(rc));
				}
			}
		}
		xyPos.nX = rc.left;
		xyPos.nY = rc.top;
		whSize.nX = rc.right - rc.left;
		whSize.nY = rc.bottom - rc.top;
	}
	//if (bWin8OrLater) {
	xyPos.nX -= 4;	// WHY are these offsets needed for a correct result???
	xyPos.nY -= 1;
	//}

	iePImage pimOrig = ieImage::Create(iePixelFormat::BGRA, whSize, true, true);
	if (!pimOrig) return false;

	BitBlt(pimOrig->BGRA()->GetDC(), 0, 0, whSize.nX + 16, whSize.nY + 16, hdc, xyPos.nX, xyPos.nY, SRCCOPY);

	pimOrig->Text()->ClearAll();
	if (bFullScreen) {
		pimOrig->Text()->Set(ieTextInfoType::Name, _T("Screenshot"));
	}
	else {
		TCHAR szWindowText[512];
		if (GetWindowText(hwnd, szWindowText, sizeof(szWindowText) / sizeof(TCHAR))) {
			for (PTCHAR p = szWindowText + _tcslen(szWindowText) - 2; p > szWindowText; p--) {	// Trim to before " - " (incl. left-right mark if Unicode...)
				if ((*p == '-') && (*(p + 1) == ' ') && ((*(p - 1) == ' ') || (*(p - 1) == 8206))) {
					if (*(p - 1) == 8206) p--;
					while ((--p > szWindowText) && (*p == ' '))
					{}
					*++p = 0;
					break;
				}
			}
			pimOrig->Text()->Set(ieTextInfoType::Name, szWindowText);
		}
	}
	pimOrig->Text()->Set(ieTextInfoType::SourceBitsPerPixel, _T("24"));

	if (pimd) pimd->Release();

	pimd = ieImageDisplay::Create(pimOrig, false);
	if (pimd) pimd->PrepareDisp();

	DeleteDC(hdc);

	return true;
}


//-----------------------------------------------------------------------------

void SetDesktopWallpaper(HWND hwnd, iePImageDisplay pimd)
{
	TCHAR sz[MAX_PATH];
	if (!mdGetSpecialFolder(eMyPicturesFolder, sz)) return;

	_stprintf(sz + _tcslen(sz), _T("\\%s desktop.bmp"), _T(PROGRAMNAME));

	//HCURSOR hcurOld = SetCursor(LoadCursor(nullptr, IDC_WAIT));

	bool r = SaveImage(hwnd, pimd, sz, false);

	//SetCursor(hcurOld);

	if (!r) return;

	SystemParametersInfo(SPI_SETDESKWALLPAPER, 0, (PVOID)sz, SPIF_SENDCHANGE);
}


//-----------------------------------------------------------------------------

static bool GetAutoLoadLinkName(PTCHAR psz, PCTCHAR pcszFile)
{
	if (!mdGetSpecialFolder(eStartupFolder, psz)) return false;

	PTCHAR pszName = psz + _tcslen(psz);
	if (pszName == psz) return false;

	if ((pszName[-1] != '\\') && (pszName[-1] != '/')) *pszName++ = '\\';

	PCTCHAR pcszName, pcszExt;
	ief_SplitPath(pcszFile, pcszName, pcszExt);

	_tcscpy(pszName, pcszName);
	_tcscat(pszName, _T(".lnk"));

	return true;
}


void RemoveAutoLoad(PCTCHAR pcszFile)
{
	TCHAR sz[MAX_PATH];
	if (!GetAutoLoadLinkName(sz, pcszFile)) return;

	ief_Delete(sz);
}


void AddAutoLoad(HWND hwnd, PCTCHAR pcszFile, float fZoom)
{
	TCHAR sz[MAX_PATH];
	if (!GetAutoLoadLinkName(sz, pcszFile)) return;

	RECT rc;
	GetWindowRect(hwnd, &rc);

	PCTCHAR pcszName, pcszExt;
	ief_SplitPath(pcszFile, pcszName, pcszExt);

	TCHAR szArgs[MAX_PATH + 64];
	_stprintf(szArgs, _T("-freeze -pos=%d,%d -zoom=%d.%d%d%d \"%s\""), rc.left, rc.top, int(fZoom), int(fZoom*10.0f) % 10, int(fZoom*100.0f) % 10, int(fZoom*1000.0f + 0.5f) % 10, pcszFile);

	mdCreateShellLink(pCfgInst->szExePath, sz, pcszName, szArgs);
}


//-----------------------------------------------------------------------------

bool ReceiveFileInFirstProcess(DWORD dwData, const void *pData, DWORD cbData)
{
	if ((dwData != C2FOURC('i', 'e', 'R', 'F')) || !pData || !cbData) return false;

	PCTCHAR pcszFileName = (PCTCHAR)pData;
	PCTCHAR pcszzOptions = pcszFileName + _tcslen(pcszFileName) + 1;
	if ((pcszzOptions - pcszFileName) * sizeof(TCHAR) > cbData) return false;

	Viewer(*pcszFileName ? pcszFileName : nullptr, *pcszzOptions ? pcszzOptions : nullptr);

	return true;
}


static BOOL CALLBACK EnumWindowsFindOne(HWND hwnd, LPARAM lParam)
{
	TCHAR sz[512];
	GetWindowText(hwnd, sz, sizeof(sz) / sizeof(TCHAR));

	if (_tcsstr(sz, _T(PROGRAMNAME))) {
		*((HWND *)lParam) = hwnd;
		return FALSE;
	}

	return TRUE;
}


bool HandOverFileToFirstProcess(PCTCHAR pcszFileName, PCTCHAR pcszzOptions)
{
	HWND hwnd = NULL;
	EnumWindows((WNDENUMPROC)EnumWindowsFindOne, LPARAM(&hwnd));
	if (!hwnd) return false;

	int n = 2, m;
	if (pcszFileName) {
		m = _tcslen(pcszFileName);
		n += m;
	}
	if (pcszzOptions) {
		for (PCTCHAR pc = pcszzOptions; *pc;) {
			m = _tcslen(pc) + 1;
			n += m;
			pc += m;
		}
	}

	PTCHAR pcData = new TCHAR[n];
	if (!pcData) return false;

	PTCHAR p = pcData;
	if (pcszFileName) {
		m = _tcslen(pcszFileName);
		memcpy(p, pcszFileName, m * sizeof(TCHAR));
		p += m;
	}
	*p++ = 0;
	if (pcszzOptions) {
		for (PCTCHAR pc = pcszzOptions; *pc;) {
			m = _tcslen(pc) + 1;
			memcpy(p, pc, m * sizeof(TCHAR));
			p += m;
			pc += m;
		}
	}
	*p++ = 0;

	COPYDATASTRUCT cds;
	cds.dwData = C2FOURC('i', 'e', 'R', 'F');
	cds.lpData = (void *)pcData;
	cds.cbData = n * sizeof(TCHAR);

	bool r = SendMessage(hwnd, WM_COPYDATA, (WPARAM)0, (LPARAM)&cds) != 0;

	delete[] pcData;

	return r;
}


//-----------------------------------------------------------------------------
// WinMain - Program entry point
//------------------------------------------------------------------------------

int PASCAL WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
	// Init config
	g_hInst = hInstance;
	new ConfigInstance();	// Sets global variables pCfg and pCfgInst

	// Parse some command line options (the command line args are also passed on to the Viewer or Index)
	PCTCHAR pcszzArgs = mdCommandLine2Args();
	if (pcszzArgs && _tcsstr(pcszzArgs, _T(".exe"))) pcszzArgs += _tcslen(pcszzArgs) + 1;

	PCTCHAR pcszFile = nullptr;
	bool bClose = false, bOnlyOne = false;

	for (PCTCHAR p = pcszzArgs; *p; p += _tcslen(p) + 1) {

		// Switch?
		if ((*p == '-') || (*p == '/')) {
			p++;
			if (!_tcsicmp(p, _T("CLOSE"))) {
				bClose = true;
			}
			else if (!_tcsicmp(p, _T("ONLYONE"))) {
				bOnlyOne = true;
			}
		}
		else {
			// We've (most likely) got a file name
			if (!pcszFile && *p) pcszFile = p;
		}
	}

	// Close previous image windows?
	if (bClose || bOnlyOne) {
		CloseAllImageEyeWindows();
		if (bClose) {
			delete pCfgInst;
			return 0;
		}
	}

	// Do we want to hand over everything to another instance?
	if (!pCfg->app.bSpawnNewProcess && (pCfg->app.iInstances > 1) && !bOnlyOne) {
		if (HandOverFileToFirstProcess(pcszFile, pcszzArgs)) {
			delete pCfgInst;
			return 0;
		}
	}

	// Init misc stuff
	g_DetectCPU();

	muiInit();

	mdAllowCopyDataMsg();

	// Add file formats
	ieAddModule(bmp);
	ieAddModule(dds);
	ieAddModule(fits);
	ieAddModule(gif);
	ieAddModule(hdr);
	ieAddModule(ico);
	ieAddModule(iff);
	ieAddModule(jpeg);
	ieAddModule(pcx);
	ieAddModule(png);
	ieAddModule(psd);
	ieAddModule(tga);
	ieAddModule(tiff);

	gie_bAutoLoadIAP = true;

	// Expand any short file name
	if (pcszFile && _tcschr(pcszFile, '~')) {
		PTCHAR psz = new TCHAR[MAX_PATH];
		mdGetLongFileName(psz, pcszFile);
		pcszFile = psz;
	}

	// Open Viewer (or possibly indirectly an Index)
	Viewer(pcszFile, pcszzArgs);

	// EoApp
#ifdef IE_SUPPORT_IMAGECACHE
	g_ieFM.ImageCache.CancelAsync();
#endif

    delete pCfgInst;

    return 0;
}
