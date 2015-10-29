//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Index window
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

#ifdef IE_SUPPORT_DIRCACHE		// No index without the enumeration functions of the directory cache...


#include <shellapi.h>
#include "mdSystem.h"
#include "muiMain.h"
#include "FileIcon.h"
#include "../Res/resource.h"

#ifndef _DEBUG
//#define __TIMEIT
#ifdef __TIMEIT
#include "mdTimer.h"
#endif
#endif


//------------------------------------------------------------------------------------------

class IndexWnd : public muiMultiWindowedAppWindow, ieIOnDirectoryChanged {

public:
	IndexWnd(PCTCHAR pcszPath, PCTCHAR pcszTitle, muiCoord xyPos, bool bStartTopMost = false);
	virtual ~IndexWnd();

	// muiMultiWindowedAppWindow
	virtual void OnCreate();
	virtual void OnDestroy();
	virtual void OnCommand(WORD wId, WORD wCode, HWND hwndFrom);
	virtual void OnContextMenu(muiCoord xyPosScr, int nButtonThatLaunchedMenu);
	virtual void OnDropFiles(HANDLE hDrop);
	virtual void OnKey(int nVirtKey, bool bKeyDown, bool bKeyRepeat, bool bAltDown);
	virtual void OnMouseMove(muiCoord xyPos, WPARAM fKeys);
	virtual void OnMouseButton(int nButton, eMouseButtonAction eAction, muiCoord xyPos, WPARAM fKeys);
	virtual void OnMouseWheel(int iDelta, muiCoord xyPosScr, WPARAM fKeys);
	virtual void OnPaint(HDC hdc, const RECT &rcDirtyRegion);
	virtual bool OnSizing(WPARAM fwWhichEdge, RECT *prcDragRect);
	virtual void OnSize(WPARAM fwSizeType, muiSize whNewSize);
	virtual void OnScroll(bool bVertical, int nScrollCode, int nPos, HWND hwndScrollBar);
	virtual void OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, int &iReturnValue);

	// ieIOnDirectoryChanged
	virtual void OnDirectoryChanged() { PostCommand(hwnd, CMD_RECREATE); }

	// Submenu callbacks
	bool OnCreateFileMenu(muiPopupMenu *pMenu);
	bool OnCreateViewMenu(muiPopupMenu *pMenu);
	bool OnCreateSortMenu(muiPopupMenu *pMenu);
	bool OnCreateCacheMenu(muiPopupMenu *pMenu);
	bool OnCreateSlideMenu(muiPopupMenu *pMenu);

protected:

	// General state
	TCHAR	szTitle[MAX_PATH+40];	// Static part of title bar test
	TCHAR	szPath[MAX_PATH];		// Path of current directory
	ieIDirectoryEnumerator *pEnum;	// Directory file name cache
	FileIconCollection fic;			// Collection of icons representing the files of szPath
	iePProgress pReadProgress;		// Progress reporting while reading image icons
	muiFont	ft;						// Text font
	BYTE	cSortIcons;				// How to sort the icons, see IconArrange2SortFunc()
	bool	bFreezedWindow;			// Don't auto-size to icons?
	static volatile unsigned nIndexes;// Number of index windows opened

	// Statistics about the icons
	struct IconCount {
		void ClearCount() { nLastUpdateTickCount = nImagesAccordingToTitleBar = nImages = nVisible = nRead = nTotal = 0; }
		int		nTotal;
		int		nRead;
		int		nVisible;
		int		nImages;
		int		nImagesAccordingToTitleBar;
		int		nLastUpdateTickCount;
	} IconCount;
	
	// For drawing of the client area
	HDC		hdcMem;					// Display context of client area bitmap
	HBITMAP	hbmpMem, hbmpOld;		// Bitmap for client area, and the old bitmap for the DC
	muiSize	whMem;					// Size of client area bitmap (always *larger* or equal to size of client area!)
	muiSize	whUsed;					// Size used to fit all icons (even those that are not visible outside of the scroll-area)
	muiSize	whDesired;				// Window size set by user by dragging window
	muiSize	whClient;				// Size of window client area
	muiSize	whExtraNca;				// Size of non-client area (left + right window frame, height of title bar + top + bottom frame)
	int		wExtraSB;				// Width of a scroll-bar
	int		iScrollY;				// Current vertical scroll-bar offset
	bool	bUseSizeFromCache;		// Use image icon size retrieved from icon cache, rather than the size in the options?
	bool	bScrollBarIsVisible;	// True if the vertical scool bar is visible (i.e. not hidden - it's always there nevertheless)
	IFileIconDrawing *pDraw;		// Drawing functions (depends on if its glass or normal)

#ifdef __TIMEIT
	mdTimer t;
#endif
	// For navigating among buttons (keyboard + mouse)
	struct Navigation {
		FileIcon *pfiPressedDown;
		FileIcon *pfiMouseHighlight;
		FileIcon *pfiKeyboardFocus;
		FileIcon *pfiLastKeyboardFocus;
		void ClearState() { pfiLastKeyboardFocus = pfiKeyboardFocus = pfiMouseHighlight = pfiPressedDown = nullptr; }
		int		iMouseWheelDelta;
		muiKeyAccelerators *pIndexKeyAccels;
		bool	bControlDown, bShiftDown;
	} Navigation;

	// Internal helper functions
	void SetPath(PCTCHAR pcszPath, bool bAddToRecent);

	void ReadIcons(bool bRecurseSubDirs = false);
	void OnReadIconsCompleted();
	void CancelReadIcons();

	void UpdateTitle();
	void UpdateWindow(FileIcon *pfiDirty = nullptr);
	void RenderIndex(HDC hDC, IFileIconDrawing *pDraw, int iScrollY, const RECT &rcDirty, FileIcon *pfiDirty = nullptr);

	void Size2Icons();
	void SortIcons();
	void OpenItem(FileIcon *pfi);

	bool SaveCache();
	bool SaveIndexImage();
};


//------------------------------------------------------------------------------------------

volatile unsigned IndexWnd::nIndexes = 0;


IndexWnd::IndexWnd(PCTCHAR pcszPath, PCTCHAR pcszTitle, muiCoord xyPos, bool bStartTopMost)
:	muiMultiWindowedAppWindow(), pEnum(nullptr),
	ft((HFONT) GetStockObject(DEFAULT_GUI_FONT)), fic(), pReadProgress(nullptr),
	hdcMem(NULL), hbmpMem(NULL), hbmpOld(NULL), 
	whMem(0, 0), whUsed(0, 0), iScrollY(0), whDesired(0, 0),
	bUseSizeFromCache(pCfg->idx.bUseSizeFromCache), bScrollBarIsVisible(true), pDraw(nullptr)
{
	// Init structs
	IconCount.ClearCount();
	Navigation.ClearState();
	Navigation.iMouseWheelDelta = 0;
	Navigation.pIndexKeyAccels = nullptr;
	Navigation.bControlDown = false;
	Navigation.bShiftDown = false;

	cSortIcons = pCfg->idx.cIconArrange;
	bFreezedWindow = pCfg->idx.bFreezedWindow;

	muiSize whSize(-1, -1);
	if (bFreezedWindow && (xyPos.x == CW_USEDEFAULT) && (xyPos.y == CW_USEDEFAULT) && (pCfg->idx.nLastXW != 0) && (pCfg->idx.nLastYH != 0)) {

		// Initialize with last size
		whSize.w = pCfg->idx.nLastXW;
		whSize.h = pCfg->idx.nLastYH;

		if (!nIndexes) {

			// Initialize with last pos (only when it's the first index window)
			xyPos.x = pCfg->idx.iLastX;
			xyPos.y = pCfg->idx.iLastY;

			// See if it needs to be adjusted to fit (display res may have changed)
			muiCoord xyDisplay;
			muiSize whDisplay;
			muiGetWorkArea(xyPos, xyDisplay, whDisplay);

			if (xyPos.x + whSize.w > xyDisplay.x + whDisplay.w) {
				xyPos.x = xyDisplay.x + whDisplay.w - whSize.w;
			}
			if (xyPos.x < xyDisplay.x) {
				xyPos.x = xyDisplay.x;
			}
			if (xyPos.x + whSize.w > xyDisplay.x + whDisplay.w) {
				whSize.w = xyDisplay.x + whDisplay.w - xyPos.x;
			}
			if (xyPos.y + whSize.h > xyDisplay.y + whDisplay.h) {
				xyPos.y = xyDisplay.y + whDisplay.h - whSize.h;
			}
			if (xyPos.y < xyDisplay.y) {
				xyPos.y = xyDisplay.y;
			}
			if (xyPos.y + whSize.h > xyDisplay.y + whDisplay.h) {
				whSize.h = xyDisplay.y + whDisplay.h - xyPos.y;
			}
		}
	}

	nIndexes++;

	// Select path (if none specific, the "current" directory is used)
	SetPath(pcszPath, true);

	// Misc.
	if (pcszTitle) _stprintf(szTitle, _T("%s - %s"), pcszTitle, _T(PROGRAMVERNAME));
	else *szTitle = 0;

	whExtraNca.w = 2 * GetSystemMetrics(SM_CXSIZEFRAME);
	whExtraNca.h = 2* GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CYCAPTION);
	if (pCfg->app.bWinVistaOrLater) {
		DWORD nExtra = 2 * GetSystemMetrics(SM_CXPADDEDBORDER);
		whExtraNca.w += nExtra;
		whExtraNca.h += nExtra;
	}
	wExtraSB = GetSystemMetrics(SM_CXVSCROLL);

#ifdef __TIMEIT
	t.Init();
#endif

	// Create window
	WindowType.bIsTopMost = bStartTopMost;
	WindowType.bAcceptsFiles = true;
	WindowType.bHasVScroll = true;

	Create(xyPos, whSize, true, szTitle, IINDEXICO);
}


IndexWnd::~IndexWnd()
{
	CancelReadIcons();

	nIndexes--;

	if (pEnum) {
		pEnum->RemoveMonitor(this);
		pEnum->Release();
		pEnum = nullptr;
	}

	if (Navigation.pIndexKeyAccels) delete Navigation.pIndexKeyAccels;

	if (hbmpOld) SelectObject(hdcMem, hbmpOld);
	if (hbmpMem) DeleteObject(hbmpMem);
	if (hdcMem) DeleteDC(hdcMem);
}


void IndexWnd::SetPath(PCTCHAR pcszPath, bool bAddToRecent)
{
	if (!pcszPath) {
		// If no path given, use current directory
		GetCurrentDirectory(MAX_PATH, szPath);
	} else {
		// Store path
		_tcscpy(szPath, pcszPath);
	}

	PCTCHAR pcszName, pcszExt;
	ief_SplitPath(szPath, pcszName, pcszExt);

	// Handle special case of shell links
	if ((pcszExt[0] == '.') && ((pcszExt[1] == 'l') || (pcszExt[1] == 'L')) && ((pcszExt[2] == 'n') || (pcszExt[2] == 'N')) && ((pcszExt[3] == 'k') || (pcszExt[3] == 'K')) && (pcszExt[4] == 0)) {
		if (mdResolveShellLink(szPath, szPath, true))
			ief_SplitPath(szPath, pcszName, pcszExt);
	}

	// Remove any trailing no \ or /
	pcszExt += _tcslen(pcszExt);
	PTCHAR pcEnd = szPath + (pcszExt-szPath);
	if ((--pcEnd >= szPath) && ((*pcEnd == '\\') || (*pcEnd == '/'))) {
		*pcEnd = 0;
	}

	// Handle special case of "path\.."
	if ((pcszName[0] == '.') && (pcszName[1] == '.') && (pcszName[2] == 0)) {
		pcEnd = szPath + (pcszName-szPath);
		*--pcEnd = 0;
		while (--pcEnd > szPath) {
			if ((*pcEnd == '\\') || (*pcEnd == '/')) {
				*pcEnd = 0;
				break;
			}
		}

	} 

	// Save path for 'recent' list
	if (bAddToRecent) pCfgInst->RecentPaths.Set(szPath);
}


void IndexWnd::OnCreate()
{
	muiAppWindow::OnCreate();

	// Init state
	SetScrollPos(hwnd, SB_VERT, 0, FALSE);

	BringWindowToForeground();

	QueueMessage(WM_COMMAND, CMD_RECREATE);
}


void IndexWnd::OnDestroy()
{
	if (bFreezedWindow && (nIndexes == 1)) {
		muiCoord xyPos;
		muiSize whSize;
		GetScreenPosOfWindow(xyPos);
		GetSizeOfAppWindow(whSize);
		if ((xyPos.x + whSize.w > 0) && (xyPos.y + whSize.h > 0)) {	//xyPos is -32000, -32000 for minimized windows
			pCfg->idx.iLastX = xyPos.x;
			pCfg->idx.iLastY = xyPos.y;
			pCfg->idx.nLastXW = whSize.w;
			pCfg->idx.nLastYH = whSize.h;
		}
	}

	muiMultiWindowedAppWindow::OnDestroy();
}


void IndexWnd::OnPaint(HDC hdc, const RECT &rcDirtyRegion)
{
	// Ensure we have drawn the window
	if (!hdcMem) {
		UpdateWindow();
		if (!hdcMem) return;
	}

	// Blit to screen!
	int iX = max(0, rcDirtyRegion.left);
	int iY = max(0, rcDirtyRegion.top);
	int iXW = min(whMem.w, rcDirtyRegion.right) - iX;
	int iYH = min(whMem.h, rcDirtyRegion.bottom) - iY;
	if ((iXW > 0) && (iYH > 0)) {
		BitBlt(hdc, iX, iY, iXW, iYH, hdcMem, iX, iY, SRCCOPY);
	}
	
	// Validate updated region
	ValidateRect(hwnd, &rcDirtyRegion);
}


void IndexWnd::UpdateWindow(FileIcon *pfiDirty)
{
	// Delayed init of icon-draw interface
	if (!pDraw) {
		if (((pCfg->idx.clrBackground.A & IE_USESOLIDBGCOLOR) == 0) && SetGlassEffect(true))
			pDraw = NewFileIconDrawGlass(ft);
		else
			pDraw = NewFileIconDrawNormal(ft);
	}

	// Get size of client area
	GetSizeOfContents(whClient);

	// Do we need to create a new memory DC+bitmap?
	bool bCreateNewBitmap = !hdcMem || !hbmpMem || (whClient.w > whMem.w) || (whClient.h > whMem.h);
	if (bCreateNewBitmap) {
		if (hbmpOld) {
			SelectObject(hdcMem, hbmpOld);
			hbmpOld = NULL;
		}
		if (hbmpMem) {
			DeleteObject(hbmpMem);
			hbmpMem = NULL;
		}
		if (hdcMem) {
			DeleteDC(hdcMem);
			hdcMem = NULL;
		}
		HDC hdcWnd = GetDC(hwnd);
		hdcMem = CreateCompatibleDC(hdcWnd);
		hbmpMem = CreateCompatibleBitmap(hdcWnd, whMem.w = whClient.w, whMem.h = whClient.h);
		hbmpOld = (HBITMAP)SelectObject(hdcMem, hbmpMem);
		ft.Select(hdcMem);
		SetBkMode(hdcMem, TRANSPARENT);
		ReleaseDC(hwnd, hdcWnd);
	}

	// Arrange icons for the window width
	IconCount.nVisible = fic.ArrangeIcons(whClient.w+wExtraSB, pDraw->GetCharHeight(), whUsed);

	// Show or hide scroll bar
	SCROLLINFO	si;
	si.cbSize = sizeof(si);		
	if (whUsed.h > whClient.h) {
		si.fMask = SIF_POS;
		GetScrollInfo(hwnd, SB_VERT, &si);
		iScrollY = si.nPos;
		si.fMask = SIF_POS | SIF_RANGE | SIF_PAGE;
		si.nMin = 0;
		si.nPos = iScrollY;
		si.nMax = whUsed.h-1;
		si.nPage = whClient.h;
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		if (!bScrollBarIsVisible) {
			ShowScrollBar(hwnd, SB_VERT, TRUE);
			bScrollBarIsVisible = true;
		}
	} else {
		si.fMask = SIF_POS;
		si.nPos = 0;
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
		iScrollY = 0;
		if (bScrollBarIsVisible) {
			ShowScrollBar(hwnd, SB_VERT, FALSE);
			bScrollBarIsVisible = false;
		}
	}
	
	// Calculate update region
	RECT rcDirty;
	if (!pfiDirty || bCreateNewBitmap) {

		// All of visible window
		rcDirty.left = 0;
		rcDirty.top = 0;
		rcDirty.right = whClient.w;
		rcDirty.bottom = whClient.h;

	} else {
		// Clip icon rectangle to window
		if (!pfiDirty->IsPlacementVisible()) 
			return;

		muiCoord xyPos;
		muiSize whSize;
		pfiDirty->GetPlacementPos(xyPos);
		pfiDirty->GetPlacementSize(whSize);
		xyPos.y -= iScrollY;

		rcDirty.left = xyPos.x;
		rcDirty.top = xyPos.y;
		rcDirty.right = rcDirty.left + whSize.w;
		rcDirty.bottom = rcDirty.top + whSize.h;

		if (rcDirty.left < 0) rcDirty.left = 0;
		if (rcDirty.top < 0) rcDirty.top = 0;
		if (rcDirty.right > whClient.w) rcDirty.right = whClient.w;
		if (rcDirty.bottom > whClient.h) rcDirty.bottom = whClient.h;
		if ((rcDirty.left >= rcDirty.right) || (rcDirty.top >= rcDirty.bottom))
			return;
	}

	// Draw it
	RenderIndex(hdcMem, pDraw, iScrollY, rcDirty, pfiDirty);
		
	// Tell windows it needs to be repainted
	InvalidateRect(hwnd, &rcDirty, FALSE);

	if (pCfg->idx.bNumFilesTitle && (IconCount.nImages != IconCount.nImagesAccordingToTitleBar)) {
		UpdateTitle();
	}
}



void IndexWnd::RenderIndex(HDC hdc, IFileIconDrawing *pDraw, int iScrollY, const RECT &rcDirty, FileIcon *pfiDirty)
{
	// Clear background
	pDraw->Fill(hdc, rcDirty, IFileIconDrawing::eClear);

	// Enumerate and paint icons (they will clip themselves)
	for (FileIcon *pfi = pfiDirty ? pfiDirty : fic.FirstIcon(); pfi; pfi = pfiDirty ? nullptr : pfi->NextIcon()) {

		if (!pfi->IsPlacementVisible()) continue;

		pfi->Draw(hdc, pDraw, iScrollY, rcDirty.bottom);
	}
}


static FileIconCollection::IconSortFunc *IconArrange2SortFunc(SBYTE cSort)
{
	switch (cSort) {
	case eArrangeNone:			return nullptr;
	case eArrangeByPath:		return FileIconCollection::ByPath;
	case eArrangeByName:		return FileIconCollection::ByName;
	case eArrangeByImageSize:	return FileIconCollection::ByImageSize;
	case eArrangeByFileSize:	return FileIconCollection::ByFileSize;
	case eArrangeByFileDate:	return FileIconCollection::ByFileDate;
	default:					return nullptr;
	}
}


void IndexWnd::ReadIcons(bool bRecurseSubDirs)
{
	// Cancel anything ongoing
	CancelReadIcons();

	for (;;) {
	  	MSG	msg;
		if (!PeekMessage(&msg, hwnd, WM_COMMAND, WM_COMMAND, PM_REMOVE)) break;
	}

	// Show progress
	UpdateTitle();

	if (!pReadProgress) pReadProgress = g_CreateProgress(hwnd);
	pReadProgress->Progress(0);
	pReadProgress->Show();
	
	SetActiveWindow(hwnd);

	HCURSOR hcurOld = SetCursor(LoadCursor(NULL, IDC_WAIT));
	
	// Read file list + start reading in icons (done asynchronously)
	IconCount.ClearCount();
	Navigation.ClearState();

	FileIconCollection::IconSortFunc *pSortFunc = IconArrange2SortFunc(cSortIcons);

	// Turn on monitoring of changes (unless we also show sub-dirs)
	if (pEnum) pEnum->RemoveMonitor(this);

	IconCount.nTotal = fic.ReadDirectory(pEnum, szPath, { bUseSizeFromCache ? 0 : pCfg->idx.iIconX, bUseSizeFromCache ? 0 : pCfg->idx.iIconY }, pSortFunc);

	if (!bRecurseSubDirs) {
		if (pEnum) pEnum->AddMonitor(this);

	}
	else if (bRecurseSubDirs) {
		IconCount.nTotal = fic.ReadSubDirectories(pSortFunc);
		
		UpdateTitle();
		
		cSortIcons = eArrangeByPath;
		fic.Sort(FileIconCollection::ByPath);
	}

	SetCursor(hcurOld);

	// Nothing to show?
	if (!IconCount.nTotal) {
		OnReadIconsCompleted();
		return;
	}

	// Shrink and paint window
	if (!bFreezedWindow) {
		Size2Icons();
		UpdateWindow();
	}

#ifdef __TIMEIT
	t.Reset();
#endif
	if (IconCount.nTotal >= (2*g_nCpuCores)) {
		g_nMultiCoreWorkRef++;
	}

	// Start reading icon images (asynchronously)
	fic.ReadImagesAsync(hwnd, WM_USER, WMU_ONREADICON);

	// ... we'll now receive WMU_ONREADICON for each icon, and our handler for that will call OnReadIconsCompleted after the last one ...
}


void IndexWnd::OnReadIconsCompleted()
{
	if (IconCount.nTotal >= (2*g_nCpuCores)) {
		g_nMultiCoreWorkRef--;
	}

#ifdef __TIMEIT
	TCHAR sz[128];
	_stprintf(sz, _T("%f"), float(t.GetDelta()));
	MessageBox(NULL, sz, sz, MB_OK);
#endif

	// Close progress
	if (pReadProgress) {
		pReadProgress->Release();
		pReadProgress = nullptr;
	}

	// Sort by image size?
	if (cSortIcons == eArrangeByImageSize)	// We haven't had image sizes until now...
		SortIcons();

	if (!bFreezedWindow) {
		
		// Shrink and paint window
		Size2Icons();
		UpdateWindow();

		// Nothing shown?
		if (!IconCount.nVisible) {	// Close if there was nothing to display
			PostMessage(hwnd, WM_COMMAND, CMD_EXIT, 0);
			return;
		}
	}
		
	// Auto-save cache?
	bool bSaveCache = pCfg->idx.bAutoCreateCache && ((IconCount.nImages && !fic.HaveCache()) || fic.IsCacheDirty());
	fic.FreeCache();

	if (bSaveCache) SaveCache();
}


void IndexWnd::CancelReadIcons()
{
	HCURSOR hcurOld = SetCursor(LoadCursor(NULL, IDC_WAIT));

	fic.CancelRead();

	if (pReadProgress) {
		pReadProgress->Release();
		pReadProgress = nullptr;
	}

	SetCursor(hcurOld);
}


void IndexWnd::Size2Icons()
{
	if (IsMaximized() || !IconCount.nTotal) return;
	if (!hdcMem) UpdateWindow();

	// Init desired client size?
	if (!whDesired.w || !whDesired.h) {
		GetSizeOfContents(whDesired);
		whDesired += whExtraNca;
	}
	int iTargetXW = whDesired.w;

	// Arrange icons
	IconCount.nVisible = fic.ArrangeIcons(iTargetXW, pDraw->GetCharHeight(), whUsed);

	// Try to shrink the width without increasing the height...
	if ((whUsed.h <= whDesired.h) && (iTargetXW > whDesired.h)) for (int iTryXW = iTargetXW; (iTryXW -= fic.GridX()) >= fic.GridX(); ) {
		muiSize whTryUsed;
		fic.ArrangeIcons(iTryXW, pDraw->GetCharHeight(), whTryUsed);
		if (whTryUsed.h <= whUsed.h) {
			whUsed = whTryUsed;
		} else {
			fic.ArrangeIcons(iTryXW+fic.GridX(), pDraw->GetCharHeight(), whTryUsed);
			break;
		}
	}

	// Find out max window size
	muiCoord xyDisplay;
	muiSize whDisplay;
	muiGetWorkArea(hwnd, xyDisplay, whDisplay);

	// Snuggle down size
	muiSize whSize = whUsed;
	whSize += whExtraNca;

	if (whSize.h > whDisplay.h) {
		whSize.h = whDisplay.h;
		whSize.w += wExtraSB;
	}
	if (whSize.w > whDisplay.w) {
		whSize.w = whDisplay.w;
	}

	MoveWindow(MUI_NOMOVE, whSize, true);
}


void IndexWnd::SortIcons()
{
	HCURSOR hcurOld = SetCursor(LoadCursor(NULL, IDC_WAIT));

	FileIconCollection::IconSortFunc *pSortFunc = IconArrange2SortFunc(cSortIcons);

	if (pSortFunc) {		

		fic.Sort(pSortFunc);

		UpdateWindow();
	}

	SetCursor(hcurOld);
}


void IndexWnd::OpenItem(FileIcon *pfi)
{
	if (!pfi) return;

	if (pfi->GetType() == fitDirectory) {

		CancelReadIcons();

		IconCount.ClearCount();
		SetPath(pfi->GetFileStr(), false);

		Navigation.ClearState();
		fic.FreeIcons();

		UpdateWindow();
		muiAppWindow::PumpMessageQueue();

		ReadIcons();

	} else {

		pfi->ShellOpenFile(hwnd);

		if (pCfg->idx.bCloseOnOpen) {
			Sleep(200);
			PostMessage(hwnd, WM_COMMAND, CMD_EXIT, 0);
		}
	}
}


void IndexWnd::OnCommand(WORD wId, WORD wCode, HWND hwndFrom)
{
	switch (wId) {
	case CMD_LOADNEW:
	case CMD_COPY:
	case CMD_MOVE:
	case CMD_DELETE:
	case CMD_ERASE:
	case CMD_RENAME:
	case CMD_EDITSLIDE:
	{

		//
		// Stuff that works on selected items
		//

		// Figure out a list of selected items
		int nNumSelected = 0, n;
		FileIcon *pfi, *pfiX;
		for (pfi = fic.FirstIcon(); pfi; pfi = pfi->NextIcon())
			if (pfi->IsStateSelected())
				nNumSelected++;

		PFileIcon *ppSelectedIcons = nullptr;
		if (nNumSelected) {
			ppSelectedIcons = new PFileIcon[nNumSelected];
			if (!ppSelectedIcons) {
				nNumSelected = 0;
			} else {
				for (pfi = fic.FirstIcon(), nNumSelected = 0; pfi; pfi = pfi->NextIcon())
					if (pfi->IsStateSelected())
						ppSelectedIcons[nNumSelected++] = pfi;
			}
		}
	
		bool bClearSelection = (ppSelectedIcons != nullptr);

		switch (wId) {

	    // Context menu:

		case CMD_LOADNEW:	// 'View' selected thumbnail

			pfiX = nullptr;
			for (n = 0; n < nNumSelected; n++) {
				pfi = ppSelectedIcons[n];
				if (pfi->GetType() == fitDirectory) {
					if (!pfiX) pfiX = pfi;
				} else {
					OpenItem(pfi);
				}
			}

			if (pfiX) {
				OpenItem(pfiX);
				bClearSelection = false;
			}
			break;

		case CMD_COPY:
		case CMD_MOVE:
		case CMD_DELETE:
		case CMD_ERASE: {

			PTCHAR pszzMultiFiles = nullptr, p;
			bool bAllOk = false;

			if (nNumSelected > 1) {

				int nNeedChars = 0;
				for (n = 0; n < nNumSelected; n++)
					nNeedChars += _tcslen(ppSelectedIcons[n]->GetFileStr());
				nNeedChars += nNumSelected + 1;

				pszzMultiFiles = new TCHAR[nNeedChars];
				if (!pszzMultiFiles) {
					nNumSelected = 1;
				} else {
					p = pszzMultiFiles;
					for (n = 0; n < nNumSelected; n++) {
						PCTCHAR pcsz = ppSelectedIcons[n]->GetFileStr();
						int nLen = _tcslen(pcsz) + 1;
						memcpy(p, pcsz, nLen*sizeof(TCHAR));
						p += nLen;
					}
					*p++ = 0;
				}
			}
			PCTCHAR pcszSrc = pszzMultiFiles ? pszzMultiFiles : ppSelectedIcons[0]->GetFileStr();

			switch (wId) {
			case CMD_COPY:
			case CMD_MOVE:
				bAllOk = ie_CopyFile(hwnd, pcszSrc, wId == CMD_MOVE, nNumSelected);
				break;
			case CMD_DELETE:
			case CMD_ERASE:
				bAllOk = ie_DeleteFileQuery(hwnd, pcszSrc, nNumSelected);
				if (bAllOk) bAllOk = ie_DeleteFile(hwnd, pcszSrc, (wId == CMD_ERASE), nNumSelected);
				break;
			default:
				bAllOk = false;
				break;
			}

			int nRemoved = 0;
			for (n = 0; n < nNumSelected; n++) {
				pfi = ppSelectedIcons[n];
				if ((wId == CMD_COPY) || ief_Exists(pcszSrc)) {
					pfi->SetStateSelected(false);
				} else {
					if (pfi->GetType() == fitImage) IconCount.nImages--;
					IconCount.nTotal--;
					IconCount.nVisible--;
					fic.DeleteIcon(pfi);
					nRemoved++;
				}
				pcszSrc += _tcslen(pcszSrc) + 1;
			}

			if (pszzMultiFiles) delete[] pszzMultiFiles;

			if (nRemoved) Navigation.ClearState();
			bClearSelection = false;
			UpdateWindow();
	   }	break;

		case CMD_RENAME:
			for (n = 0; n < nNumSelected; n++) {
				pfi = ppSelectedIcons[n];
				TCHAR szFile[MAX_PATH];
				_tcscpy(szFile, pfi->GetFileStr());
				if (!ie_RenameFile(hwnd, szFile)) 
					break;
				fic.RenameIcon(pfi, szFile);
			}
			SortIcons();
			break;

		case CMD_EDITSLIDE:		// Edit slideshow...
			for (n = 0; n < nNumSelected; n++) {
				pfi = ppSelectedIcons[n];
				if (pfi->GetType() != fitSlideshow) continue;
				mdNotepad(ppSelectedIcons[0]->GetFileStr());
				break;
			}
			break;
		}

		// Clear slection state
		if (ppSelectedIcons) {
			if (bClearSelection) {
				for (n = 0; n < nNumSelected; n++)
					ppSelectedIcons[n]->SetStateSelected(false);
				UpdateWindow((nNumSelected == 1) ? ppSelectedIcons[0] : NULL);
			}
			delete[] ppSelectedIcons;
		}

	}	return;
	}

	//
	// Stuff that doesn't work on selected items:
	//

	switch (wId) {

	case CMD_SPAWNNEW:
		SpawnViewer(nullptr, hwnd);
		break;

	case CMD_KILLEMALL:
		CloseAllImageEyeWindows();
		break;	
		
	case CMD_MANUALRECREATE:
		bUseSizeFromCache = false;
		fic.FreeIcons();
	case CMD_RECREATE:
		ReadIcons();
		break;

	case CMD_RECURSESUBDIRS:
		muiAppWindow::PumpMessageQueue();	// Get it to redraw so that the context menu is hidden

		cSortIcons = eArrangeNone;
		ReadIcons(true);
		break;
		
	case CMD_SAVECACHE:
		if (!SaveCache())
			MessageBox(hwnd, _T("Couldn't create cache file!"), _T(PROGRAMNAME), MB_OK);
		break;

	case CMD_REMOVECACHE:
		fic.RemoveCache();
		bUseSizeFromCache = false;
		break;

	case CMD_SLIDESHOW:
	case CMD_SLIDEAUTO: {
		TCHAR szScriptFile[MAX_PATH];
		if (!CreateSlideScript(szScriptFile, hwnd, szPath, wId == CMD_SLIDEAUTO)) break;
		SpawnViewer(szScriptFile);
	}	break;
		
	case CMD_SLIDEFULLSCREEN:
		pCfg->sli.bRunMaximized = !pCfg->sli.bRunMaximized;
		break;

	case CMD_SAVEAS:
		SaveIndexImage();
		break;
		
	case CMD_FULLSCREENTOGGLE:
		ShowWindow(hwnd, IsMaximized() ? SW_RESTORE : SW_MAXIMIZE);
		break;

	case CMD_PROPERTIES: {
		int iOrigIconX = pCfg->idx.iIconX;
		int iOrigIconY = pCfg->idx.iIconY;
		ieBGRA clrOriginalBackground = pCfg->idx.clrBackground;

		if (!pCfgInst->ShowOptionsDialog(hwnd, ConfigInstance::eIndexOptions)) {
			break;
		}

		if (pCfg->app.bWinVistaOrLater && (pCfg->idx.clrBackground != clrOriginalBackground)) {
			QueueMessage(WM_DWMCOMPOSITIONCHANGED);
		}

		if ((iOrigIconX != pCfg->idx.iIconX) || (iOrigIconY != pCfg->idx.iIconY)) {
			bUseSizeFromCache = false;
			fic.FreeIcons();
			ReadIcons();
			UpdateWindow();
		} else {
			if (!fic.HaveCache() && pCfg->idx.bAutoCreateCache)
				SaveCache();
			UpdateTitle();
		}

	}	break;

	case CMD_FREEZE:
		pCfg->idx.bFreezedWindow = bFreezedWindow = !bFreezedWindow;
		if (!pCfg->idx.bFreezedWindow) {
			pCfg->idx.nLastYH = pCfg->idx.nLastXW = 0;
		}
		break;

	case CMD_NEXTWINDOW:
		NextPrevWindow(hwnd, false);
		break;
	case CMD_PREVWINDOW:
		NextPrevWindow(hwnd, true);
		break;

	case CMD_EXIT:
		DestroyWindow(hwnd);
		break;
		
	case CMD_HELP:
		OpenHelp(hwnd);
		break;

	case CMD_SORT_PATH:
	case CMD_SORT_NAME:
	case CMD_SORT_IMAGESIZE:
	case CMD_SORT_FILESIZE:
	case CMD_SORT_FILEDATE:
		switch (wId) {
		case CMD_SORT_PATH:			pCfg->idx.cIconArrange = cSortIcons = eArrangeByPath;		break;
		case CMD_SORT_NAME:			pCfg->idx.cIconArrange = cSortIcons = eArrangeByName;		break;
		case CMD_SORT_IMAGESIZE:	pCfg->idx.cIconArrange = cSortIcons = eArrangeByImageSize;	break;
		case CMD_SORT_FILESIZE:		pCfg->idx.cIconArrange = cSortIcons = eArrangeByFileSize;	break;
		case CMD_SORT_FILEDATE:		pCfg->idx.cIconArrange = cSortIcons = eArrangeByFileDate;	break;
		}
		SortIcons();
		break;
		
	case CMD_SHOW_DIRS:
	case CMD_SHOW_OTHER:
	case CMD_SHOW_NAME1:
	case CMD_SHOW_NAME2:
	case CMD_SHOW_NAME3:
	case CMD_SHOW_NAME4:
	case CMD_SHOW_COMMENT:
	case CMD_SHOW_RES:
	case CMD_SHOW_EXT:
	case CMD_SHOW_DATE:
	case CMD_SHOW_SIZE:
		switch (wId) {
		case CMD_SHOW_DIRS:		pCfg->idx.bShowDirs = !pCfg->idx.bShowDirs;			break;
		case CMD_SHOW_OTHER:	pCfg->idx.bShowOther = !pCfg->idx.bShowOther;		break;
		case CMD_SHOW_NAME1:
		case CMD_SHOW_NAME2:
		case CMD_SHOW_NAME3:
		case CMD_SHOW_NAME4: {	BYTE nLines = 1 + BYTE(wId - CMD_SHOW_NAME1);
								pCfg->idx.cShowNameLines = (pCfg->idx.cShowNameLines != nLines) ? nLines : 0;
							 }	break;							 
		case CMD_SHOW_COMMENT:	pCfg->idx.bShowComment = !pCfg->idx.bShowComment;	break;
		case CMD_SHOW_RES:		pCfg->idx.bShowRes = !pCfg->idx.bShowRes;			break;
		case CMD_SHOW_EXT:		pCfg->idx.bShowExt = !pCfg->idx.bShowExt;			break;
		case CMD_SHOW_DATE:		pCfg->idx.bShowDate = !pCfg->idx.bShowDate;			break;
		case CMD_SHOW_SIZE:		pCfg->idx.bShowSize = !pCfg->idx.bShowSize;			break;
		}
		UpdateWindow();
		break;
	}
}


bool IndexWnd::OnSizing(WPARAM fwWhichEdge, RECT *prcDragRect)
{ 
	if (prcDragRect) {
		whDesired.w = prcDragRect->right - prcDragRect->left;
		whDesired.h = prcDragRect->bottom - prcDragRect->top;
	}

	return false; 
}


void IndexWnd::OnSize(WPARAM fwSizeType, muiSize whNewSize)
{
	switch (fwSizeType) {
	case SIZE_RESTORED:
	case SIZE_MAXIMIZED:
		UpdateWindow();
		break;
	}
}


void IndexWnd::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, int &iReturnValue)
{
	switch (uMsg) {

	case WM_USER:
		// Internal messaging:
		switch (wParam) {

		case WMU_ONREADICON: {

			IconCount.nRead++;
			if (((FileIcon *)lParam)->GetType() == fitImage) {
				IconCount.nImages++;
			}

			int nCurTickCount = GetTickCount();
			bool bUpdateEveryTime = (IconCount.nTotal <= 256);
			bool bAllDone = (IconCount.nRead == IconCount.nTotal);

			if (bUpdateEveryTime || bAllDone || (nCurTickCount - IconCount.nLastUpdateTickCount >= 250)) {	// Throttle UpdateWindow which is O(n) due to icon arrangement calculations. Since we get reports from O(n) icons, we'd get O(n^2) complexity...

				IconCount.nLastUpdateTickCount = nCurTickCount;

				if (pReadProgress) pReadProgress->Progress(IconCount.nRead, IconCount.nTotal);

				UpdateWindow(bUpdateEveryTime ? (FileIcon *)lParam : nullptr);
			}

			if (bAllDone) {
				OnReadIconsCompleted();
			}
		}	break;
	
		case WMU_ONCACHESAVED:	// Finished saving cache -> start monitoring changes again (was temporary on hold)
			if (pEnum) pEnum->AddMonitor(this);
			break;
		}

		iReturnValue = 0;
		break;

	case WM_COPYDATA:
		ReceiveFileInFirstProcess(PCOPYDATASTRUCT(lParam)->dwData, PCOPYDATASTRUCT(lParam)->lpData, PCOPYDATASTRUCT(lParam)->cbData);
		iReturnValue = TRUE;
		break;

	case WM_DWMCOMPOSITIONCHANGED: {
		
		bool bGlass = ((pCfg->idx.clrBackground.A & IE_USESOLIDBGCOLOR) == 0) && SetGlassEffect(true);
		if (!bGlass) SetGlassEffect(false);

		if (pDraw) {
			delete pDraw;
			pDraw = nullptr;
		}

		UpdateWindow();
	}	break;

	}
}


void IndexWnd::OnKey(int nVirtKey, bool bKeyDown, bool bKeyRepeat, bool bAltDown)
{
	muiAppWindow::OnKey(nVirtKey, bKeyDown, bKeyRepeat, bAltDown);

	if (!bKeyDown) {
		switch (nVirtKey) {

		case VK_RETURN:
			if (!Navigation.pfiKeyboardFocus || !Navigation.pfiKeyboardFocus->IsStatePressed()) return;
			Navigation.pfiKeyboardFocus->SetStatePressed(false);
			UpdateWindow(Navigation.pfiKeyboardFocus);
			OpenItem(Navigation.pfiKeyboardFocus);			
			return;
	
		case VK_CONTROL:
			Navigation.bControlDown = false;
			return;

		case VK_SHIFT:
			Navigation.bShiftDown = false;
			return;
		}

		return;
	}

	switch (nVirtKey) {

	case VK_ESCAPE:
		DestroyWindow(hwnd);
		return;

	case VK_LEFT:
	case VK_RIGHT:
	case VK_UP:
	case VK_DOWN:
	case VK_PRIOR:
	case VK_NEXT:
	case VK_HOME:
	case VK_END:
		break;

	case VK_SPACE:
		nVirtKey = VK_NEXT;
		break;

	case VK_RETURN:
		if (!Navigation.pfiKeyboardFocus) {
			// Just re-set keyboard focus
			if (Navigation.pfiLastKeyboardFocus) break;
			return;
		}
		if (Navigation.bControlDown || Navigation.bShiftDown) {
			// Toggle selection state
			Navigation.pfiKeyboardFocus->SetStateSelected(!Navigation.pfiKeyboardFocus->IsStateSelected());
			UpdateWindow(Navigation.pfiKeyboardFocus);
			return;
		}
		// Press down button
		Navigation.pfiKeyboardFocus->SetStatePressed(true);
		UpdateWindow(Navigation.pfiKeyboardFocus);
		return;

	case VK_CONTROL:
		Navigation.bControlDown = true;
		return;

	case VK_SHIFT:
		Navigation.bShiftDown = true;
		return;

	default:
		return;
	}

	// Move focus according to keyboard navigation keys
	FileIcon *pfiPrev = Navigation.pfiKeyboardFocus, *pfiNew;
	int nHorz, nVert;
	int nIconsPerRow = whUsed.w / fic.GridX();
	int nRowsPerPage = whClient.h / fic.GridY();
	bool bScrollUp = false, bScrollDown = false;

	switch (nVirtKey) {

	case VK_HOME:
		pfiNew = fic.FirstIcon();
		while (pfiNew && !pfiNew->IsPlacementVisible())
			pfiNew = pfiNew->NextIcon();
		break;

	case VK_END:
		pfiNew = fic.FirstIcon();
		if (!pfiNew) break;
		while (pfiNew->NextIcon())
			pfiNew = pfiNew->NextIcon();
		while (pfiNew && !pfiNew->IsPlacementVisible())
			pfiNew = pfiNew->PrevIcon();
		break;

	case VK_LEFT:
	case VK_UP:
	case VK_PRIOR:
		bScrollUp = (nVirtKey != VK_PRIOR);
		pfiNew = pfiPrev;
		if (!pfiNew) {
			pfiNew = Navigation.pfiLastKeyboardFocus;
			if (!pfiNew) {
				pfiNew = fic.FirstIcon();
				while (pfiNew && !pfiNew->IsPlacementVisible())
					pfiNew = pfiNew->NextIcon();
			}
			if (nVirtKey != VK_PRIOR) break;
			break;
		}
		nHorz = (nVirtKey == VK_LEFT) ? 1 : ((nVirtKey == VK_UP) ? nIconsPerRow : (nRowsPerPage*nIconsPerRow));
		for (FileIcon *pfiEnum = pfiNew; nHorz; ) {
			FileIcon *pfiPrev = pfiEnum->PrevIcon();
			if (!pfiPrev) break;
			pfiEnum = pfiPrev;
			if (!pfiEnum->IsPlacementVisible()) continue;
			if (!--nHorz) pfiNew = pfiEnum;
		}
		break;

	case VK_RIGHT:
	case VK_DOWN:
	case VK_NEXT:
		bScrollDown = (nVirtKey != VK_NEXT);
		pfiNew = pfiPrev;
		if (!pfiNew) {
			pfiNew = Navigation.pfiLastKeyboardFocus;
			if (!pfiNew) {
				pfiNew = fic.FirstIcon();
				while (pfiNew && !pfiNew->IsPlacementVisible())
					pfiNew = pfiNew->NextIcon();
			}
			if (nVirtKey != VK_NEXT) break;
		}
		nHorz = (nVirtKey == VK_RIGHT) ? 1 : ((nVirtKey == VK_DOWN) ? nIconsPerRow : (nRowsPerPage*nIconsPerRow));
		for (FileIcon *pfiEnum = pfiNew; nHorz; ) {
			FileIcon *pfiNext = pfiEnum->NextIcon();
			if (!pfiNext) break;
			pfiEnum = pfiNext;
			if (!pfiEnum->IsPlacementVisible()) continue;
			if (!--nHorz) pfiNew = pfiEnum;
		}
		break;

	case VK_RETURN:
		pfiNew = Navigation.pfiLastKeyboardFocus;
		break;
	}

	// Update window
	if (pfiPrev != pfiNew) {
		if (pfiPrev) {
			pfiPrev->SetStateFocused(false);
			UpdateWindow(pfiPrev);
			Navigation.pfiLastKeyboardFocus = pfiPrev;
		}
		if (pfiNew) {
			pfiNew->SetStateFocused(true);
			UpdateWindow(pfiNew);
			Navigation.pfiKeyboardFocus = pfiNew;
		}
	}

	// Scroll so that it's visible?
	if (Navigation.pfiKeyboardFocus && bScrollBarIsVisible) {
		muiCoord xyPos;
		Navigation.pfiKeyboardFocus->GetPlacementPos(xyPos);
		if ((xyPos.y < iScrollY) || (xyPos.y >= (iScrollY + whClient.h - fic.GridY()/2))) {
			iScrollY = xyPos.y;
			if (iScrollY < fic.GridY()) iScrollY = 0;
			if (bScrollUp || bScrollDown) {
				if (bScrollDown) {
					iScrollY -= (whClient.h - fic.GridY());
					if (iScrollY < 0) iScrollY = 0;
				}
			}
			if (iScrollY > (whUsed.h-whClient.h)) iScrollY = whUsed.h-whClient.h;

			SCROLLINFO	si;
			si.cbSize = sizeof(si);		
			si.fMask = SIF_POS;
			si.nPos = iScrollY;
			SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
			UpdateWindow();
		}
	}
}


void IndexWnd::OnMouseWheel(int iDelta, muiCoord xyPosScr, WPARAM fKeys)
{
	if (!bScrollBarIsVisible) return;

	if (iDelta < 0) {
		if (Navigation.iMouseWheelDelta > 0)
			Navigation.iMouseWheelDelta = 0;		// Reset acc if rotation dir has changed
		
	} else if (iDelta > 0) {

		if (Navigation.iMouseWheelDelta < 0)
			Navigation.iMouseWheelDelta = 0;		// Reset acc if rotation dir has changed

	} else {
		return;
	}

	Navigation.iMouseWheelDelta += iDelta;		// Accumulate change

	int nScrollAmount = Navigation.iMouseWheelDelta / WHEEL_DELTA;
	if (!nScrollAmount) return;

	Navigation.iMouseWheelDelta -= nScrollAmount*WHEEL_DELTA;

	iScrollY -= nScrollAmount*(fic.GridY()/2);

	SCROLLINFO si;
	si.cbSize = sizeof(si);
	si.fMask = SIF_POS;
	si.nPos = iScrollY;
	SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

	UpdateWindow();
}


void IndexWnd::OnScroll(bool bVertical, int nScrollCode, int nPos, HWND hwndScrollBar)
{
	if (!bVertical) return;

	SCROLLINFO si;
	si.cbSize = sizeof(si);

	for (;;) {

		// Scroll...
		si.fMask = SIF_POS;
		GetScrollInfo(hwnd, SB_VERT, &si);

		switch (nScrollCode) {
		case SB_LINEUP:
			si.nPos -= (fic.GridY()/3);
			break;
		case SB_LINEDOWN:
			si.nPos += (fic.GridY()/3);
			break;
		case SB_PAGEUP:
		case SB_PAGEDOWN: {
			RECT 	rc;
			GetClientRect(hwnd, &rc);
			int iDelta = fic.GridY()*(rc.bottom/fic.GridY());
			if (nScrollCode == SB_PAGEUP) iDelta = -iDelta;
			si.nPos += iDelta;
		}	break;
		case SB_TOP:
			si.nPos = 0;
			break;
		case SB_BOTTOM:
			si.nPos = 0x7FFFFFFF;
			break;
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION:
			si.fMask = SIF_TRACKPOS;
			GetScrollInfo(hwnd, SB_VERT, &si);
			si.nPos = si.nTrackPos;
			break;
		}
		si.fMask = SIF_POS;
		SetScrollInfo(hwnd, SB_VERT, &si, TRUE);

		// Try to merge all vertical-scroll messages before painting
	  	MSG	msg;
		if (!PeekMessage(&msg, hwnd, WM_VSCROLL, WM_VSCROLL, PM_REMOVE)) break;

		nScrollCode = LOWORD(msg.wParam);
		nPos = HIWORD(msg.wParam);
		hwndScrollBar = (HWND)msg.lParam;
	} 

	UpdateWindow();

	muiAppWindow::PumpMessageQueue();	// Get it to paint now!
}


void IndexWnd::OnDropFiles(HANDLE hDrop)
// Pop up new windows with any files dropped in the current window
{
	TCHAR	szSrc[MAX_PATH], szDst[MAX_PATH];
	int		i, iNoFiles = DragQueryFile((HDROP)hDrop, 0xFFFFFFFF, NULL, 0);

	for (i = 0; i < iNoFiles; i++) {

		DragQueryFile((HDROP)hDrop, i, szSrc, sizeof(szSrc));

		PCTCHAR pcszName, pcszExt;
		ief_SplitPath(szSrc, pcszName, pcszExt);

		_stprintf(szDst, _T("%s\\%s"), szPath, pcszName);

		if (!ie_CopyFile(hwnd, szDst, szSrc)) continue;

		FileIcon *pfi = fic.AddFile(szDst);
		if (pfi) {
			IconCount.nTotal++;
			if (pfi->GetType() == fitImage) IconCount.nImages++;
		}
	}

	DragFinish((HDROP)hDrop);

	SortIcons();
}


bool IndexWnd::SaveIndexImage()
{
	if (!IconCount.nVisible) return false;

	// Create DC + bitmap + image
	HDC hdc = CreateCompatibleDC(NULL);
	if (!hdc) return false;

   struct {
	    BITMAPINFOHEADER	bmih;
    } bmi;
	ZeroMemory(&bmi.bmih, sizeof(bmi.bmih));
	bmi.bmih.biSize = sizeof(bmi.bmih);
	bmi.bmih.biWidth = whUsed.w;
	bmi.bmih.biHeight = -whUsed.h;
	bmi.bmih.biPlanes = 1;
	bmi.bmih.biBitCount = 32;
	bmi.bmih.biCompression = BI_RGB;
	bmi.bmih.biClrUsed = 0;

	PBYTE pbBitmap = nullptr;

	HBITMAP hbmp = CreateDIBSection(hdc, (BITMAPINFO *)&bmi, DIB_RGB_COLORS, (void **)&pbBitmap, NULL, 0);
	if (!hbmp) return false;
	
	HBITMAP hbmpOld = (HBITMAP)SelectObject(hdc, hbmp);

	iePImage pimIndex = ieImage::Create(iePixelFormat::BGRA, { DWORD(whUsed.w), DWORD(whUsed.h) }, true, false);
	if (!pimIndex) return false;

	// Draw to bitmap then copy to image
	IFileIconDrawing *pDrawNormal = NewFileIconDrawNormal(ft);
	RECT rc = { 0, 0, whUsed.w, whUsed.h };

	RenderIndex(hdc, pDrawNormal, 0, rc);

	delete pDrawNormal;

	BitBlt(pimIndex->BGRA()->GetDC(), 0, 0, whUsed.w+4, whUsed.h+4, hdc, -4, -1, SRCCOPY);

	// Create display image wrapper
	iePImageDisplay pimdIndex = ieImageDisplay::Create(pimIndex, false);

	PCTCHAR pcszName, pcszExt;
	ief_SplitPath(szPath, pcszName, pcszExt);
	if (!*pcszName)	pcszName = _T("index");
	pimdIndex->Text()->Set(ieTextInfoType::SourceFile, pcszName);		// Set a name that SaveImage will present as the default default name

	// Save
	SaveImage(hwnd, pimdIndex, nullptr, true);

	// Clean up
	pimdIndex->Release();
	SelectObject(hdc, hbmpOld);
	DeleteObject(hbmp);
	DeleteDC(hdc);
	
	return true;
}


void IndexWnd::UpdateTitle()
{
	TCHAR sz[MAX_PATH+64], *p;
	PCTCHAR pcszName, pcszExt;

	if (*szTitle) {
		_tcscpy(sz, szTitle);
	} else {
		p = szPath;
		if (!pCfg->idx.bFullPathTitle) {
			ief_SplitPath(p, pcszName, pcszExt);
			if (!*pcszName) pcszName = szPath;		// E.g., a root 'C:' is an unnamed directory...
			p += (pcszName - p);
		}
		_tcscpy(sz, p);
		if (sz[0] && (sz[1] == ':')) {				// Fix up drive name
			sz[0] = _totupper(sz[0]);
			if (!sz[2]) {
				sz[2] = '\\';
				sz[3] = 0;
			}
		}
	}
	p = sz + _tcslen(sz);

	if (fic.IncludesSubDirectories()) {
		_tcscpy(p, _T(" + ..."));			p += _tcslen(p);
	}

	if (pCfg->idx.bNumFilesTitle) {
		IconCount.nImagesAccordingToTitleBar = IconCount.nImages;
		if (IconCount.nImagesAccordingToTitleBar > 0) {
			_stprintf(p, _T(" | %d"), IconCount.nImagesAccordingToTitleBar);	
			p += _tcslen(p);
		}
	}
	_tcscpy(p, _T(" | "));
	p += 3;
	_tcscpy(p, ieTranslate(_T("Index")));	
	p += _tcslen(p);
	_tcscpy(p, _T(" | "));
	p += 3;
	_tcscpy(p, _T(PROGRAMVERNAME));

	SetWindowText(hwnd, sz);
}


bool IndexWnd::SaveCache()
{
	if (fic.IncludesSubDirectories()) return false;

	if (pEnum) pEnum->RemoveMonitor(this);

	fic.SaveCache(hwnd, WM_USER, WMU_ONCACHESAVED);

	return true;
}


static bool cbOnCreateFileMenu(void *pContext, muiPopupMenu *pMenu)
{
	return ((IndexWnd *)pContext)->OnCreateFileMenu(pMenu);
}

static bool cbOnCreateViewMenu(void *pContext, muiPopupMenu *pMenu)
{
	return ((IndexWnd *)pContext)->OnCreateViewMenu(pMenu);
}

static bool cbOnCreateSortMenu(void *pContext, muiPopupMenu *pMenu)
{
	return ((IndexWnd *)pContext)->OnCreateSortMenu(pMenu);
}

static bool cbOnCreateCacheMenu(void *pContext, muiPopupMenu *pMenu)
{
	return ((IndexWnd *)pContext)->OnCreateCacheMenu(pMenu);
}

static bool cbOnCreateSlideMenu(void *pContext, muiPopupMenu *pMenu)
{
	return ((IndexWnd *)pContext)->OnCreateSlideMenu(pMenu);
}


void IndexWnd::OnContextMenu(muiCoord xyPosScr, int nButtonThatLaunchedMenu)
{
	muiPopupMenu *pMenu = new muiPopupMenu();
	if (!pMenu) return;

	if (Navigation.pfiKeyboardFocus) {
		// Called from keyboard, use pos from focus
		muiCoord xy;
		muiSize wh;
		Navigation.pfiKeyboardFocus->GetPlacementPos(xy);
		xy.y -= iScrollY;
		if ((xy.y >= 0) && (xy.y < whClient.h)) {
			Navigation.pfiKeyboardFocus->GetPlacementSize(wh);
			GetScreenPosOfWindow(xyPosScr);
			xyPosScr += xy;
			xyPosScr.x += wh.w;
			xyPosScr.y += fic.GridY()/3;
		}
		if (!Navigation.pfiKeyboardFocus->IsStateSelected()) {
			Navigation.pfiKeyboardFocus->SetStateSelected(true);
			UpdateWindow(Navigation.pfiKeyboardFocus);
		}
	}

	pMenu->Create(hwnd, xyPosScr, { 1, 1 }, &ft, nButtonThatLaunchedMenu);

	pMenu->AddHeader(IEYEICO, NULL, 0, NULL, LOGOICO);

	if (!Navigation.pIndexKeyAccels) Navigation.pIndexKeyAccels = new muiKeyAccelerators(INDEX_MENU);
	pMenu->MapStatusTextsFromKeyAccel(Navigation.pIndexKeyAccels);


	int nImages = 0, nSlides = 0, nDirs = 0, nOthers = 0;
	for (FileIcon *pfi = fic.FirstIcon(); pfi; pfi = pfi->NextIcon()) {
		if (!pfi->IsStateSelected()) continue;
		switch (pfi->GetType()) {
		case fitImage:		nImages++;	break;
		case fitSlideshow:	nSlides++;	break;
		case fitDirectory:	nDirs++;	break;
		default:			nOthers++;	break;
		}
	}

	if (nImages) {
		pMenu->AddCommand(NEWICO, ieTranslate(_T("View image")), MUI_COLOR_BLACK, hwnd, CMD_LOADNEW);
	}
	if (nSlides) {
		pMenu->AddCommand(SLIDEICO, ieTranslate(_T("View slideshow")), MUI_COLOR_BLACK, hwnd, CMD_LOADNEW);
		pMenu->AddCommand(0, ieTranslate(_T("Edit slideshow")), MUI_COLOR_BLACK, hwnd, CMD_EDITSLIDE);
	}
	if (nDirs) {
		pMenu->AddCommand(INDEXICO, ieTranslate(_T("View index")), MUI_COLOR_BLACK, hwnd, CMD_LOADNEW);
	}
	if (nOthers) {
		pMenu->AddCommand(0, ieTranslate(_T("Open")), MUI_COLOR_BLACK, hwnd, CMD_LOADNEW);
	}

	pMenu->AddCommand(OPENICO, ieTranslate(_T("Spawn new...")), MUI_COLOR_BLACK, hwnd, CMD_SPAWNNEW);
	pMenu->AddSubMenu(0, ieTranslate(_T("File")), MUI_COLOR_BLACK, cbOnCreateFileMenu, this);
	if (!fic.IncludesSubDirectories() && !pReadProgress) pMenu->AddCommand(0, ieTranslate(_T("Recurse sub-directories")), MUI_COLOR_BLACK, hwnd, CMD_RECURSESUBDIRS);
	pMenu->AddSeparator();

	pMenu->AddSubMenu(0, ieTranslate(_T("View")), MUI_COLOR_BLACK, cbOnCreateViewMenu, this);
	pMenu->AddSubMenu(0, ieTranslate(_T("Sort icons")), MUI_COLOR_BLACK, cbOnCreateSortMenu, this);
	pMenu->AddSubMenu(0, ieTranslate(_T("Icon cache")), MUI_COLOR_BLACK, cbOnCreateCacheMenu, this);
	pMenu->AddSubMenu(0, ieTranslate(_T("Slideshow")), MUI_COLOR_BLACK, cbOnCreateSlideMenu, this);

	pMenu->AddCommand(OPTIONSICO, ieTranslate(_T("Options")), MUI_COLOR_BLACK, hwnd, CMD_PROPERTIES, 0, !pReadProgress);
	pMenu->AddSeparator();

	pMenu->AddCommand(HELPICO, ieTranslate(_T("Help")), MUI_COLOR_BLACK, hwnd, CMD_HELP);
	pMenu->AddCommand(EXITICO, ieTranslate(_T("Close")), MUI_COLOR_BLACK, hwnd, CMD_EXIT);

	pMenu->AddSeparator(true);

	pMenu->TrackPopup(true);
}


bool IndexWnd::OnCreateFileMenu(muiPopupMenu *pMenu)
{
	pMenu->MapStatusTextsFromKeyAccel(Navigation.pIndexKeyAccels);

	pMenu->AddSeparator(true);

	int nFiles = 0;
	int nDirs = 0;
	for (FileIcon *pfi = fic.FirstIcon(); pfi; pfi = pfi->NextIcon()) {
		if (!pfi->IsStateSelected()) continue;
		if (pfi->GetType() != fitDirectory) {
			nFiles++;
		} else {
			if (_tcsicmp(pfi->GetFileNameStr(), _T("..")) != 0)
				nDirs++;
		}
	}

	if (nFiles) {
		pMenu->AddCommand(0, ieTranslate(_T("Copy file...")), MUI_COLOR_BLACK, hwnd, CMD_COPY);
		pMenu->AddCommand(0, ieTranslate(_T("Move file...")), MUI_COLOR_BLACK, hwnd, CMD_MOVE);
	}
	if (nFiles || nDirs) {
		pMenu->AddCommand(0, ieTranslate(_T("Rename file...")), MUI_COLOR_BLACK, hwnd, CMD_RENAME);
	}
	if (nFiles) {
		pMenu->AddCommand(0, ieTranslate(_T("Delete file...")), MUI_COLOR_BLACK, hwnd, CMD_DELETE);
		pMenu->AddCommand(0, ieTranslate(_T("Erase file...")), MUI_COLOR_BLACK, hwnd, CMD_ERASE);
	}
	if (nFiles || nDirs) {
		pMenu->AddSeparator();
	}

	pMenu->AddCommand(SAVEICO, ieTranslate(_T("Save index as bitmap...")), MUI_COLOR_BLACK, hwnd, CMD_SAVEAS);

	pMenu->AddSeparator(true);

	return true;
}


bool IndexWnd::OnCreateViewMenu(muiPopupMenu *pMenu)
{
	IndexOptions *pidx = &pCfg->idx;

	pMenu->MapStatusTextsFromKeyAccel(Navigation.pIndexKeyAccels);

	pMenu->AddSeparator(true);

	pMenu->AddCommand(pidx->bShowDirs ? CHECKMARKICO : 0, ieTranslate(_T("Directories")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_DIRS);
	pMenu->AddCommand(pidx->bShowOther ? CHECKMARKICO : 0, ieTranslate(_T("Non-image files")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_OTHER);
	pMenu->AddCommand(bFreezedWindow ? CHECKMARKICO : 0, ieTranslate(_T("Freeze window")), MUI_COLOR_BLACK, hwnd, CMD_FREEZE);
	pMenu->AddSeparator();

	pMenu->AddCommand((pidx->cShowNameLines == 1) ? CHECKMARKICO : 0, ieTranslate(_T("Names (1 line)")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_NAME1);
	pMenu->AddCommand((pidx->cShowNameLines == 2) ? CHECKMARKICO : 0, ieTranslate(_T("Names (2 lines)")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_NAME2);
	pMenu->AddCommand((pidx->cShowNameLines == 3) ? CHECKMARKICO : 0, ieTranslate(_T("Names (3 lines)")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_NAME3);
	pMenu->AddCommand((pidx->cShowNameLines == 4) ? CHECKMARKICO : 0, ieTranslate(_T("Names (4 lines)")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_NAME4);
	pMenu->AddCommand(pidx->bShowComment ? CHECKMARKICO : 0, ieTranslate(_T("Comment")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_COMMENT);
	pMenu->AddSeparator();

	pMenu->AddCommand(pidx->bShowRes ? CHECKMARKICO : 0, ieTranslate(_T("Resolutions")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_RES);
	pMenu->AddCommand(pidx->bShowExt ? CHECKMARKICO : 0, ieTranslate(_T("File extensions")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_EXT);
	pMenu->AddCommand(pidx->bShowSize ? CHECKMARKICO : 0, ieTranslate(_T("File sizes")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_SIZE);
	pMenu->AddCommand(pidx->bShowDate ? CHECKMARKICO : 0, ieTranslate(_T("File dates")), MUI_COLOR_BLACK, hwnd, CMD_SHOW_DATE);

	pMenu->AddSeparator(true);

	return true;
}


bool IndexWnd::OnCreateSortMenu(muiPopupMenu *pMenu)
{
	IndexOptions *pidx = &pCfg->idx;

	pMenu->MapStatusTextsFromKeyAccel(Navigation.pIndexKeyAccels);

	pMenu->AddSeparator(true);

	pMenu->AddCommand(((cSortIcons == eArrangeByName) || (!fic.IncludesSubDirectories() && (cSortIcons == eArrangeByPath))) ? CHECKMARKICO : 0, ieTranslate(_T("By name")), MUI_COLOR_BLACK, hwnd, CMD_SORT_NAME);
	if (fic.IncludesSubDirectories()) {
		pMenu->AddCommand((cSortIcons == eArrangeByPath) ? CHECKMARKICO : 0, ieTranslate(_T("By full path")), MUI_COLOR_BLACK, hwnd, CMD_SORT_PATH);
	}
	pMenu->AddCommand((cSortIcons == eArrangeByImageSize) ? CHECKMARKICO : 0, ieTranslate(_T("By image size")), MUI_COLOR_BLACK, hwnd, CMD_SORT_IMAGESIZE);
	pMenu->AddCommand((cSortIcons == eArrangeByFileSize) ? CHECKMARKICO : 0, ieTranslate(_T("By file size")), MUI_COLOR_BLACK, hwnd, CMD_SORT_FILESIZE);
	pMenu->AddCommand((cSortIcons == eArrangeByFileDate) ? CHECKMARKICO : 0, ieTranslate(_T("By file date")), MUI_COLOR_BLACK, hwnd, CMD_SORT_FILEDATE);

	pMenu->AddSeparator(true);

	return true;
}


bool IndexWnd::OnCreateCacheMenu(muiPopupMenu *pMenu)
{
	IndexOptions *pidx = &pCfg->idx;

	pMenu->MapStatusTextsFromKeyAccel(Navigation.pIndexKeyAccels);

	pMenu->AddSeparator(true);

	pMenu->AddCommand(RELOADICO, ieTranslate(_T("Recreate")), MUI_COLOR_BLACK, hwnd, CMD_MANUALRECREATE);
	if (!pidx->bAutoCreateCache) {
		if (!fic.HaveCache()) pMenu->AddCommand(SAVEICO, ieTranslate(_T("Save cache")), MUI_COLOR_BLACK, hwnd, CMD_SAVECACHE);
		if (fic.IsCacheDirty()) pMenu->AddCommand(SAVEICO, ieTranslate(_T("Update cache")), MUI_COLOR_BLACK, hwnd, CMD_SAVECACHE);
		if (fic.HaveCacheFile()) pMenu->AddCommand(DELETEICO, ieTranslate(_T("Remove cache")), MUI_COLOR_BLACK, hwnd, CMD_REMOVECACHE);
	}

	return true;
}


bool IndexWnd::OnCreateSlideMenu(muiPopupMenu *pMenu)
{
	pMenu->MapStatusTextsFromKeyAccel(Navigation.pIndexKeyAccels);

	pMenu->AddSeparator(true);

	// Slideshow menu
	pMenu->AddCommand(SLIDEICO, ieTranslate(_T("Slideshow")), MUI_COLOR_BLACK, hwnd, CMD_SLIDEAUTO);
	pMenu->AddCommand(OPTIONSICO, ieTranslate(_T("Create slideshow script")), MUI_COLOR_BLACK, hwnd, CMD_SLIDESHOW);
	pMenu->AddCommand(pCfg->sli.bRunMaximized ? CHECKMARKICO : 0, ieTranslate(_T("Run in fullscreen mode")), MUI_COLOR_BLACK, hwnd, CMD_SLIDEFULLSCREEN);
	
	pMenu->AddSeparator(true);

	return true;
}


void IndexWnd::OnMouseButton(int nButton, eMouseButtonAction eAction, muiCoord xyPos, WPARAM fKeys)
{
	muiAppWindow::OnMouseButton(nButton, eAction, xyPos, fKeys);

	if (nButton > 2) {
		// Extended button:
		if ((nButton == 3) && (eAction == eButtonUp)) {
			// Browse back button
			FileIcon *pfi = fic.FirstIcon();
			if (pfi && (pfi->GetType() == fitDirectory) && !_tcscmp(pfi->GetFileNameStr(), _T(".."))) {
				OpenItem(pfi);
			}
		}
		return;
	}

	// Button 0..2: Is it up or down?
	bool bDown;
	switch (eAction) {
	case eButtonDown:
		bDown = true;
		break;
	case eButtonUp:
		if (!Navigation.pfiPressedDown) return;
		bDown = false;
		break;
	case eButtonDoubleClick:
		if (nButton != 0) return;
		break;
	default:
		return;
	}

    bool bRightButton = (nButton == 1);
	bool bSelectKeys = ((fKeys & (MK_CONTROL | MK_SHIFT)) != 0) || (nButton == 2);

	// Find icon button beneath cursor
	xyPos.y += iScrollY;
	FileIcon *pfi = fic.FirstIcon();
	for (; pfi; pfi = pfi->NextIcon())
		if (pfi->HitTest(xyPos))
			break;

	// Double-click?
	if (eAction == eButtonDoubleClick) {
		if (!pfi) ShowWindow(hwnd, IsMaximized() ? SW_RESTORE : SW_MAXIMIZE);	// Toggle maximized, if double-click outside of icon
		else SetActiveWindow(NULL);												// Restore just opened image to focus, if double-click outside on icon
		return;
	}

	if (bSelectKeys) {
		// Control or shift pressed down: just toggle selection
		if (pfi) {
			pfi->SetStateSelected(!pfi->IsStateSelected());
			UpdateWindow(pfi);
			return;
		}
	} else if (!bRightButton) {
		// Clear selections
		for (FileIcon *pfiSel = fic.FirstIcon(); pfiSel; pfiSel = pfiSel->NextIcon()) {
			if (!pfiSel->IsStateSelected()) continue;
			pfiSel->SetStateSelected(false);
			if (pfiSel != pfi) UpdateWindow(pfiSel);
		}
	}

	if (Navigation.pfiPressedDown && (pfi != Navigation.pfiPressedDown)) {
		// Clicked in something other then the previously down button: Unpress it
		Navigation.pfiPressedDown->SetStateClearAll();
		UpdateWindow(Navigation.pfiPressedDown);
		Navigation.pfiPressedDown = nullptr;
	}

	if (!pfi) return;

	if (bDown) {
		// Button down: Set button as pressed & selected
		Navigation.pfiPressedDown = pfi;
		Navigation.pfiLastKeyboardFocus = pfi;
		pfi->SetStateSelected(true);
		pfi->SetStatePressed(true);
		UpdateWindow(pfi);
#ifdef IE_SUPPORT_IMAGECACHE
		// Cache the image (the user will most likely release the button to view it, and then we'll already have started on the work)
		if (pfi->GetType() == fitImage) {
			g_ieFM.ImageCache.CacheImageAsync(pfi->GetFileStr());
		}
#endif
	} else {
		// Button up: Remove pressed state
		bool bWasPressed = pfi->IsStatePressed();
		if (bWasPressed) {
			pfi->SetStatePressed(false);
			if (bRightButton) UpdateWindow(pfi);
			Navigation.pfiPressedDown = nullptr;
		}
		if (bRightButton) {
			// Right click up: Keep selection, we'll then get a WM_CONTEXTMENU
		} else {
			// Left button up: Open file or directory
			pfi->SetStateFocused(false);
			UpdateWindow(pfi);
			if (bWasPressed) OpenItem(pfi);
		}
	} 
}


void IndexWnd::OnMouseMove(muiCoord xyPos, WPARAM fKeys)
{
	muiAppWindow::OnMouseMove(xyPos, fKeys);

	// Find icon button beneath cursor
	xyPos.y += iScrollY;

	FileIcon *pfi = fic.FirstIcon();
	for (; pfi; pfi = pfi->NextIcon())
		if (pfi->HitTest(xyPos))
			break;

	// Set mouse highlight (focus state)
	if (pfi != Navigation.pfiMouseHighlight) {
		if (Navigation.pfiMouseHighlight) {
			Navigation.pfiMouseHighlight->SetStateFocused(false);
			UpdateWindow(Navigation.pfiMouseHighlight);
			Navigation.pfiMouseHighlight = nullptr;
		}
		if (pfi) {
			if (Navigation.pfiKeyboardFocus) {	// Remove keyboard focus?
				if (Navigation.pfiKeyboardFocus != pfi) {
					Navigation.pfiKeyboardFocus->SetStateFocused(false);
					UpdateWindow(Navigation.pfiKeyboardFocus);
				}
				Navigation.pfiLastKeyboardFocus = Navigation.pfiKeyboardFocus;
				Navigation.pfiKeyboardFocus = nullptr;
			}
			Navigation.pfiMouseHighlight = pfi;
			Navigation.pfiMouseHighlight->SetStateFocused(true);
			UpdateWindow(Navigation.pfiMouseHighlight);
		}
	}

	// Show/hide pressed state?
	if (Navigation.pfiPressedDown) {
		bool bSelectKeys = (fKeys & (MK_CONTROL | MK_SHIFT)) != 0;
		bool bUpdate = false;
		if (pfi == Navigation.pfiPressedDown) {
			if (!bSelectKeys) {
				if (!Navigation.pfiPressedDown->IsStateSelected()) {
					Navigation.pfiPressedDown->SetStateSelected(true);
					bUpdate = true;
				}
			}
			if (!Navigation.pfiPressedDown->IsStatePressed()) {
				Navigation.pfiPressedDown->SetStatePressed(true);
				bUpdate = true;
			}
		} else {
			if (!bSelectKeys) {
				if (Navigation.pfiPressedDown->IsStateSelected()) {
					Navigation.pfiPressedDown->SetStateSelected(false);
					bUpdate = true;
				}
			}
			if (Navigation.pfiPressedDown->IsStatePressed()) {
				Navigation.pfiPressedDown->SetStatePressed(false);
				bUpdate = true;
			}
		}
		if (bUpdate) UpdateWindow(Navigation.pfiPressedDown);
	}
}


//-----------------------------------------------------------------------------

bool SpawnIndex(PCTCHAR pcszFolder)
{
	if (!pcszFolder || !*pcszFolder) return false;

	if (pCfg->app.bSpawnNewProcess) {

		TCHAR szCmd[9 + IE_FILENAMELEN];
		_stprintf(szCmd, _T("-index \"%s\""), pcszFolder);
		pcszFolder = szCmd;

		if (!mdSpawnExe(pCfgInst->szExePath, pcszFolder, false, false)) {
			return false;
		}
	}
	else {

		Index(pcszFolder, nullptr);
	}

	return true;
}


//-----------------------------------------------------------------------------

void Index(PCTCHAR pcszFileName, PCTCHAR pcszzOptions)
{
	PCTCHAR pcszTitle = nullptr;
	int iXPos = CW_USEDEFAULT, iYPos = CW_USEDEFAULT;
	bool bTopMost = false;

	if (pcszzOptions) for (PCTCHAR p = pcszzOptions; *p; p += _tcslen(p) + 1) {

		if ((*p != '-') && (*p != '/')) continue; 		// Switch?
		p++;

		if (!_tcsnicmp(p, _T("POS="), 4)) {
			p += 4;
			iXPos = _tstoi(p);
			while ((*p != ',') && (*p != ' ') && (*p != 0)) p++;
			iYPos = _tstoi(++p);
		}
		else if (!_tcsnicmp(p, _T("TITLE="), 6)) {
			pcszTitle = p + 6;
		}
		else if (!_tcsicmp(p, _T("TOPMOST"))) {
			bTopMost = true;
		}
	}

	IndexWnd *pWnd = new IndexWnd(pcszFileName, pcszTitle, { iXPos, iYPos }, bTopMost);

	pWnd->SetAccelerators(INDEX_MENU);

	pWnd->Run();
}


#else //!IE_SUPPORT_DIRCACHE

void Index(PCTCHAR pcszFileName, PCTCHAR pcszzOptions)
{
	return;
}

#endif //!IE_SUPPORT_DIRCACHE
