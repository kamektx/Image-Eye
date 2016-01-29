//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Viewer window
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
#include "ieColor.h"
#include "muiMain.h"
#include "mdSystem.h"
#include "../Res/resource.h"

extern HBRUSH CreateAlphaBrush(ieBGRA clr);						// in FileIconDraw.cpp
extern ieTextInfoType OverlayType2InfoType(int nOverlayType);	// in Options.cpp


//-----------------------------------------------------------------------------

static bool GetFolderForImage(PTCHAR pszPath, const iePImageDisplay pimd)
{
	if (!pimd || !pimd->Text()) return false;

	PCTCHAR pcszFile;
	pcszFile = pimd->Text()->Get(ieTextInfoType::SourceFile);
	if (!pcszFile || !*pcszFile) return false;

	PCTCHAR pcszName, pcszExt;
	ief_SplitPath(pcszFile, pcszName, pcszExt);
	int nPathLen = pcszName - pcszFile;

	if (nPathLen) {
		memcpy(pszPath, pcszFile, nPathLen*sizeof(TCHAR));
	}
	else {
		GetCurrentDirectory(MAX_PATH, pszPath);
		nPathLen = _tcslen(pszPath);
	}

	if ((pszPath[nPathLen - 1] == '\\') || (pszPath[nPathLen - 1] == '/')) nPathLen--;
	pszPath[nPathLen] = 0;

	return true;
}


//-----------------------------------------------------------------------------

static void AdjustImageCacheSize(unsigned nViewerWindows)
{
#ifdef IE_SUPPORT_IMAGECACHE
	unsigned nChacheSize;
	if (nViewerWindows <= 1) nChacheSize = 3;
	else if (nViewerWindows <= 3) nChacheSize = 4 + nViewerWindows;
	else nChacheSize = 5 + nViewerWindows;

	g_ieFM.ImageCache.SetSize(nChacheSize);
#endif
}


//-----------------------------------------------------------------------------

class ViewerWnd : public muiMultiWindowedAppWindow, ieIViewer {
public:
	ViewerWnd(PCTCHAR pcszFileName, PCTCHAR pcszTitle = nullptr, muiCoord xyPos = { CW_USEDEFAULT, CW_USEDEFAULT }, float fZoomToFactor = -1.0f, bool bStartFullscreen = false, bool bStartFrozen = false, bool bStartTopMost = false);
	bool HaveImage() const { return (pimd != nullptr); }
	virtual ~ViewerWnd();

	// muiMultiWindowedAppWindow
	virtual void OnActivateApp(bool bItIsThisApp);
	virtual void OnCreate();
	virtual void OnCommand(WORD wId, WORD wCode, HWND hwndFrom);
	virtual void OnContextMenu(muiCoord xyPosScr, int nButtonThatLaunchedMenu);
	virtual void OnDestroy();
	virtual void OnDropFiles(HANDLE hDrop);
	virtual void OnKey(int nVirtKey, bool bKeyDown, bool bKeyRepeat, bool bAltDown);
	virtual void OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, int &iReturnValue);
	virtual void OnMouseMove(muiCoord xyPos, WPARAM fKeys);
	virtual void OnMouseButton(int nButton, eMouseButtonAction eAction, muiCoord xyPos, WPARAM fKeys); 
	virtual void OnMouseWheel(int iDelta, muiCoord xyPosScr, WPARAM fKeys);
	virtual void OnPaint(HDC hdc, const RECT &rcDirtyRegion);
	virtual void OnSetCursor(HWND hwndChild = NULL, WORD nHittest = HTCLIENT, WORD wMouseMsg = WM_MOUSEMOVE);
	virtual bool OnPosChanging(WINDOWPOS *pNewState);
	virtual bool OnSizing(WPARAM fwWhichEdge, RECT *prcDragRect);
	virtual void OnSize(WPARAM fwSizeType, muiSize whSize);
	virtual void OnMove(muiCoord xyNewPos);
	virtual void OnTimer(WPARAM wTimerID);

	// ieIViewer
	virtual bool LoadNewImage(PCTCHAR pcszFile, bool bDisplayProgress = true, bool bCacheNextImage = false, bool bShowErrors = true);
	virtual HWND GetHWnd() const { return hwnd; }

	// Internal callbacks
	bool OnCreateFileMenu(muiPopupMenu *pMenu);
	bool OnCreateViewMenu(muiPopupMenu *pMenu);
	bool OnCreateViewZoomMenu(muiPopupMenu *pMenu);
	bool OnCreateSlideshowMenu(muiPopupMenu *pMenu);
	void UpdateWindow(const muiCoord *pxyNewPos = nullptr, const muiSize *pwhNewSize = nullptr, bool bContentsChanged = true) { if (WindowType.bLayered) UpdateLayered(pxyNewPos, pwhNewSize, bContentsChanged); else UpdatePainted(pxyNewPos, pwhNewSize, bContentsChanged); }

protected:

	void UpdateCaption(bool bCaptioned, bool bForceUpdate = false);
	void UpdateTitle();
	void UpdateGlassState(bool bEnable);
	void UpdatePainted(const muiCoord *pxyNewPos = nullptr, const muiSize *pwhNewSize = nullptr, bool bContentsChanged = true);
	void UpdateLayered(const muiCoord *pxyNewPos = nullptr, const muiSize *pwhNewSize = nullptr, bool bContentsChanged = true);
	void UpdateWorkArea();
	void UpdateSystemMetrics();

	void ZoomImage(double dZoom);
	void Trim2Window();

	void Size2Image(bool bCenterOnLast = true);
	void Size2Original();
	void Size2Window(bool bThenSize2Image, bool bAllowLargerThanWorkArea = false, muiCoord *pxyNewPos = nullptr, muiSize *pwhNewSize = nullptr);
	void OnNewImage();

	bool LoadIt(PCTCHAR pcszFile, bool bCacheNextImage, bool bShowErrors = true);
	bool LoadNextPrevImage(ieSearchDirection eDirection, bool bSpawnNewViewer = false, bool bCacheNext = false);

	static volatile unsigned nViewers;		// Number of viewer windows opened
	
	iePImageDisplay pimd;					// Image
	ieIDirectoryEnumerator *pEnum;			// Directory cache

	muiWindow *pOwnerOfFrozen;				// Frozen window has a dummy parent window, to prevent showing up in the taskbar
	muiPopupMenu *pMenu;					// Contect menu
	muiFont	ft;								// Font for drawing overlay texts
	HBRUSH hbrBorderBrush;					// Brush for drawing backgrounds
	ieBGRA clrBorderBrush;					// Color of hbrBorderBrush

	bool	bFrozen;						// Are we in froozen window mode?
	bool	bAutoCaptionTimer;				// Has timer been set fora mouse tracking for auto-hide caption feature?
	bool	bGlassBackground;				// Do we currently have a glass background enabled?
	bool	bAlphaImage;					// Do we currently have an image with alpha-transparency?
	bool	bSlideShow;						// Currently running a slide-show?

	muiSize whClient;						// Window client area size
	ieXY	xyPan;							// Image panning offsets

	muiCoord xyWorkOrigo;					// Display work area origo
	muiSize whWorkArea;						// Display work area size

    int		iMouseWheelDelta;				// Mouse wheel delta-tick counter
    muiCoord xyLastMouseDrag;				// Last position when dragging with mouse
    bool	bMouseCaptured;					// Is mouse captured for dragging?
	bool	bCtrlDown, bShiftDown, bAltDown, bMidDown;	// Is the key down?
	bool	bInhibitAutoSize;				// Tweak to temporarily disable auto size to image when user manually zooms in to > display res
	volatile int iInhibitCaptionChange;		// When this counter is non-zero, we shouldn't change the caption...
	volatile int iInhibitSizeChange;		// When this counter is non-zero, we shouldn't change the window size...
	bool	bPreventDrawToDesktop;			// Prevent drawing to the desktop (for Windows XP, to avoid painting on top of properties and adjust image dialogs)
	bool	bPreventContextMenu;			// Prevent showing context menu
	bool	bRecenterWindow;				// Recenter window on monitor upon new image?
	bool	bComingFromFullscreen;			// Resize window to fit image?
	bool	bDontSize2WindowOnRestore;		// Was the image loaded in fullscreen mode?
	bool	bPaintBackgroundOnly;			// Paint background only, not the image, in OnPaint()
	bool	bGoFullscreenOnFirst;			// Command line or config option to start fullscreen
	float	fInitZoomFactor;				// Command line option to initialize zoom
	muiKeyAccelerators *pViewerKeyAccels, *pFrozenKeyAccels;	// Keyboard accelerators
		
	int		nShowWaitCursor;				// Are we currently waiting for something?
	bool	bMultiMonitor;					// Running with multiple monitors?
	int		ciFrameXW, ciFrameYH, ciCaptionYH, ciMinWinXW;	// Window frame width/heaight, caption height, min width of captioned window
};


//------------------------------------------------------------------------------------------

class DummyOwnerWnd : public muiWindow {
public:
	DummyOwnerWnd()
	:	muiWindow() 
	{
		Create(nullptr, { 0, 0 }, { 4, 4 }, false, false);
	}
};


volatile unsigned ViewerWnd::nViewers = 0;


//-----------------------------------------------------------------------------

ViewerWnd::ViewerWnd(PCTCHAR pcszFileName, PCTCHAR pcszTitle, muiCoord xyPos, float fZoomToFactor, bool bStartFullscreen, bool bStartFrozen, bool bStartTopMost)
:	muiMultiWindowedAppWindow(),
	pimd(nullptr), pEnum(nullptr), pOwnerOfFrozen(nullptr), pMenu(nullptr), ft((HFONT) GetStockObject(DEFAULT_GUI_FONT)),
	bFrozen(false), bSlideShow(false), bAutoCaptionTimer(false), bGlassBackground(false), bAlphaImage(false), fInitZoomFactor(fZoomToFactor),
	whClient{ 0, 0 }, xyPan{ 0, 0 },
	iMouseWheelDelta(0), bMouseCaptured(false), bCtrlDown(false), bShiftDown(false), bAltDown(false), bMidDown(false),
	iInhibitCaptionChange(0), iInhibitSizeChange(0), bPreventDrawToDesktop(false),
	bPreventContextMenu(false), bGoFullscreenOnFirst(bStartFullscreen || pCfg->vwr.bRunMaximized), bComingFromFullscreen(false), bDontSize2WindowOnRestore(false), bPaintBackgroundOnly(false),
	bRecenterWindow((xyPos.x == CW_USEDEFAULT) || (xyPos.y == CW_USEDEFAULT)),
	pViewerKeyAccels(nullptr), pFrozenKeyAccels(nullptr), hbrBorderBrush(NULL),
	nShowWaitCursor(0)
{
	WindowType.bAcceptsFiles = true;
	WindowType.bIsTopMost = bStartTopMost;
	WindowType.bHasCaption = true;
	WindowType.bLayered = pCfg->vwr.bHideCaption;

	muiSize whSize(2, 2);
	UpdateWorkArea();
	UpdateSystemMetrics();

	AdjustImageCacheSize(++nViewers);

	if (bStartFrozen) {

		// If in frozen-mode, we don't want it to appear in the taskbar - create a dummy owner window that is now visible...
		pOwnerOfFrozen = new DummyOwnerWnd();
		if (xyPos.x == CW_USEDEFAULT) xyPos.x = 0;
		if (xyPos.y == CW_USEDEFAULT) xyPos.y = 0;
		Create(xyPos, whSize, false, _T(""), IEYEICO, pOwnerOfFrozen->WindowHandle());

		UpdateCaption(false, true);

		SetWindowPos(hwnd, HWND_BOTTOM, xyPos.x, xyPos.y, 0, 0, SWP_NOACTIVATE | SWP_NOSIZE);

		RemoveAutoLoad(pcszFileName);

		bFrozen = true;

	}
	else {

		if (bRecenterWindow) {
			xyPos.x = xyWorkOrigo.x + ((whWorkArea.w - whSize.w) >> 1);
			xyPos.y = xyWorkOrigo.y + ((whWorkArea.h - whSize.h) >> 1);
		}

		// Create our image window
		Create(xyPos, whSize, false, _T(PROGRAMVERNAME), IEYEICO);

		SetVisible(true); // Must do this to get it into the task bar while open file dialog is up
	}

	// Decode the image (also prompt for a file name if need be)
	if (!LoadNewImage(pcszFileName, true, !bFrozen)) {
		return;
	}
	bRecenterWindow = true;

	// Override window title?
	if (pcszTitle) {
		TCHAR sz[MAX_PATH+64];
		_stprintf(sz, _T("%s - %s"), pcszTitle, _T(PROGRAMVERNAME));
		SetWindowText(hwnd, sz);
	}
}


ViewerWnd::~ViewerWnd()
{
	if (pimd) {
		pimd->Release();
		pimd = nullptr;
	}

	if (pEnum) {
		pEnum->Release();
		pEnum = nullptr;
	}

	AdjustImageCacheSize(--nViewers);

	if (pOwnerOfFrozen) pOwnerOfFrozen->Close();

	if (pViewerKeyAccels) delete pViewerKeyAccels;
	if (pFrozenKeyAccels) delete pFrozenKeyAccels;

	if (hbrBorderBrush) DeleteObject(hbrBorderBrush);
}


void ViewerWnd::UpdateSystemMetrics()
{
	bMultiMonitor = (GetSystemMetrics(SM_CMONITORS) > 1);
	ciFrameXW = GetSystemMetrics(SM_CXSIZEFRAME);
	ciFrameYH = GetSystemMetrics(SM_CYSIZEFRAME);
	if (pCfg->app.bWinVistaOrLater) {
		DWORD nExtra = GetSystemMetrics(SM_CXPADDEDBORDER);
		ciFrameXW += nExtra;
		ciFrameYH += nExtra;
	}
	ciCaptionYH = GetSystemMetrics(SM_CYCAPTION);
	ciMinWinXW = GetSystemMetrics(SM_CXMIN);
}


void ViewerWnd::OnActivateApp(bool bItIsThisApp)
{
	bAltDown = false;

	if (!bItIsThisApp && !bPreventDrawToDesktop && IsMaximized()) {
		if (bMultiMonitor) {
			for (int nTries = 50; nTries--; ) {
				HWND hwndNew = GetForegroundWindow();
				if (hwndNew) {
					RECT rcOur, rcNew;
					GetWindowRect(hwnd, &rcOur);
					GetWindowRect(hwndNew, &rcNew);
					if ((rcNew.right <= rcOur.left+10) || (10+rcNew.left >= rcOur.right) || (rcNew.bottom <= rcOur.top+10) || (10+rcNew.top >= rcOur.bottom))
						return;
					break;
				}
				Sleep(10);
			}
		}

		// If loosing active status in full-screen mode, then automatically minimize to tray
		ShowWindow(hwnd, SW_MINIMIZE);
	}
}


void ViewerWnd::OnCreate()
{
	muiAppWindow::OnCreate();
}


void ViewerWnd::OnCommand(WORD wId, WORD wCode, HWND hwndFrom)
{
	bAltDown = false;
	bShiftDown = false;
	if (!pimd) return;

	if (bFrozen) {
		// Frozen window special command handling:

		switch (wId) {

		case CMD_FREEZE:
			bFrozen = false;
			SetAccelerators(VIEWER_MENU);
			UpdateCaption(true);
			break;

		case CMD_SPAWNNEW:
			SpawnViewer(nullptr, hwnd);
			break;
			
		case CMD_LOADNEW:
			LoadNewImage(nullptr, true, false);
			break;
		
		case CMD_NEXTIMAGE:
			LoadNextPrevImage(ieSearchDirection::NextFile);
			break;
		case CMD_PREVIMAGE:
			LoadNextPrevImage(ieSearchDirection::PrevFile);
			break;
		case CMD_FIRSTIMAGE:
			LoadNextPrevImage(ieSearchDirection::FirstFile);
			break;
		case CMD_LASTIMAGE:
			LoadNextPrevImage(ieSearchDirection::LastFile);
			break;

		case CMD_ONNEWIMAGE:
			OnNewImage();
			break;

		case CMD_ONSHOWHIDEWAITCURSOR:
			nShowWaitCursor += (wCode != 0) ? 1 : -1;
			OnSetCursor();
			break;

		case CMD_ZOOM_IN:
		case CMD_ZOOM_OUT:
			switch (wId) {
			case CMD_ZOOM_IN:	ZoomImage(pimd->Adjustments()->GetZoom() * 1.1892071150027210667);	break;	//1.189... = 2^0.25
			case CMD_ZOOM_OUT:	ZoomImage(pimd->Adjustments()->GetZoom() / 1.1892071150027210667);	break; // I.e. zoom 4 times for factor 2x
			}
		  	break;

		case CMD_HELP:
			OpenHelp(hwnd);
			break;

		case CMD_EXIT:
			SendCommand(hwnd, CMD_FREEZE);
			DestroyWindow(hwnd);
			break;
		}

		return;
	}


	// Normal (non-frozen window) command handling:
		
	switch (wId) {

	// Main menu:

	case CMD_EXIT:
		bFrozen = false;
		DestroyWindow(hwnd);
		break;

	case CMD_SPAWNNEW:
		iInhibitCaptionChange++;
		SpawnViewer(nullptr, hwnd);
		iInhibitCaptionChange--;
		break;

	case CMD_LOADNEW:
		iInhibitCaptionChange++;
		LoadNewImage(nullptr, true, true);
		iInhibitCaptionChange--;
		break;

	case CMD_PROPERTIES:
	case CMD_FILEINFO: {
	   iInhibitCaptionChange++;
	   bPreventDrawToDesktop = true;

	   ieBGRA clrOriginalBackground = pCfg->vwr.clrBackground;
	   bool bOriginalAutoSizeImage = pCfg->vwr.bAutoSizeImage;

	   pCfgInst->ShowOptionsDialog(hwnd, ConfigInstance::eViewerOptions, pimd, wId == CMD_FILEINFO);

	   if (pCfg->app.bWinVistaOrLater && (pCfg->vwr.clrBackground != clrOriginalBackground)) {
		   QueueMessage(WM_DWMCOMPOSITIONCHANGED);
	   }

	   if (pCfg->vwr.bAutoSizeImage && !bOriginalAutoSizeImage) {
		   Size2Window(true);
	   }

	   bPreventDrawToDesktop = false;
	   iInhibitCaptionChange--;

	   UpdateWindow();
	}	break;

	case CMD_HELP:
		OpenHelp(hwnd);
		break;

	// File menu:

	case CMD_NEXTIMAGE:
		LoadNextPrevImage(ieSearchDirection::NextFile, false, true);
		break;
	case CMD_PREVIMAGE:
		LoadNextPrevImage(ieSearchDirection::PrevFile, false, true);
		break;
	case CMD_SPAWNNEXTIMAGE:
		LoadNextPrevImage(ieSearchDirection::NextFile, true);
		break;
	case CMD_SPAWNPREVIMAGE:
		LoadNextPrevImage(ieSearchDirection::PrevFile, true);
		break;
	case CMD_FIRSTIMAGE:
		LoadNextPrevImage(ieSearchDirection::FirstFile, false, true);
		break;
	case CMD_LASTIMAGE:
		LoadNextPrevImage(ieSearchDirection::LastFile, false, true);
		break;
	case CMD_NEXTWINDOW:
		NextPrevWindow(hwnd, false);
		break;
	case CMD_PREVWINDOW:
		NextPrevWindow(hwnd, true);
		break;

	case CMD_INDEX: {

		TCHAR szPath[MAX_PATH];
		PCTCHAR pszPath = nullptr;
		if (pimd && GetFolderForImage(szPath, pimd))
			pszPath = szPath;

		if (!SpawnIndex(pszPath)) break;

		if (pCfg->idx.bCloseOnOpen) {
			PostMessage(hwnd, WM_COMMAND, CMD_EXIT, 0);
		}
	}	break;
        
	case CMD_SLIDESHOW:
	case CMD_SLIDEAUTO: {
		TCHAR szPath[MAX_PATH];
		if (!GetFolderForImage(szPath, pimd)) break;

		TCHAR szScriptFile[MAX_PATH];
		if (!CreateSlideScript(szScriptFile, hwnd, szPath, wId == CMD_SLIDEAUTO)) break;

		LoadNewImage(szScriptFile, false, false);
	}	break;

	case CMD_SLIDECANCEL:
		PostMessage(hwnd, WM_USER, wCode, 0);
		break;

	case CMD_SLIDEFULLSCREEN:
		pCfg->sli.bRunMaximized = !pCfg->sli.bRunMaximized;
		break;

	case CMD_SAVEAS:
		iInhibitCaptionChange++;
		SaveImage(hwnd, pimd, nullptr, false);
		iInhibitCaptionChange--;
		break;

	case CMD_RELOAD:
		if (!pimd->Text()->Have(ieTextInfoType::SourceFile)) break;
		iInhibitCaptionChange++;
#ifdef IE_SUPPORT_IMAGECACHE
		g_ieFM.ImageCache.FlushFile(pimd->Text()->Get(ieTextInfoType::SourceFile));
#endif
		LoadNewImage(pimd->Text()->Get(ieTextInfoType::SourceFile), true, false);
		iInhibitCaptionChange--;break;
		
	case CMD_COPY:
		ie_CopyFile(hwnd, pimd->Text()->Get(ieTextInfoType::SourceFile));
		break;
	
	case CMD_MOVE:
		if (ie_CopyFile(hwnd, pimd->Text()->Get(ieTextInfoType::SourceFile), true)) {
			pimd->Text()->Set(ieTextInfoType::SourceFile, _T(""));
		}
		break;

	case CMD_RENAME: {
		iInhibitCaptionChange++;
		PCTCHAR pcszFile = pimd->Text()->Get(ieTextInfoType::SourceFile);
		if (*pcszFile) {
			TCHAR szFile[MAX_PATH];
			_tcscpy(szFile, pcszFile);
			if (ie_RenameFile(hwnd, szFile)) {
				pimd->Text()->Set(ieTextInfoType::SourceFile, szFile);
				UpdateTitle();
			}
		}
		iInhibitCaptionChange--;
	}	break;
		
	case CMD_DELETE:
	case CMD_ERASE:
		iInhibitCaptionChange++;
		if (ie_DeleteFileQuery(hwnd, pimd->Text()->Get(ieTextInfoType::SourceFile))) {
			ie_DeleteFile(hwnd, pimd->Text()->Get(ieTextInfoType::SourceFile), (wId == CMD_ERASE));
		}
		iInhibitCaptionChange--;
		break;
			
	case CMD_DELETE_SHOWNEXT:
	case CMD_ERASE_SHOWNEXT: {
		iInhibitCaptionChange++;
		if (ie_DeleteFileQuery(hwnd, pimd->Text()->Get(ieTextInfoType::SourceFile))) {
			TCHAR szFileName[MAX_PATH];
			_tcscpy(szFileName, pimd->Text()->Get(ieTextInfoType::SourceFile));
			if (!LoadNextPrevImage(ieSearchDirection::NextFile, false, true)) PostCommand(hwnd, CMD_LOADNEW);
			if (*szFileName != 0) ie_DeleteFile(hwnd, szFileName, (wId == CMD_ERASE_SHOWNEXT));
		}
		iInhibitCaptionChange--;
	}	break;
		
	case CMD_KILLEMALL:
		CloseAllImageEyeWindows();
		break;
		
	case CMD_CLIPBCOPY:
		Clipboard_Write(hwnd, pimd->OrigImage());
		break;

	case CMD_CLIPBPASTE:
		if (Clipboard_Read(*this)) {
			pimd->Text()->Set(ieTextInfoType::SourceFile, _T("<clipboard>"));
			if (pEnum) {
				pEnum->Release();
				pEnum = nullptr;
			}
		}
		break;
		
	case CMD_CAPTURESCREEN:
	case CMD_CAPTUREWINDOW:
		ShowWindow(hwnd, SW_MINIMIZE);
		if (CaptureWindow(pimd, wId == CMD_CAPTURESCREEN)) {
			UpdateTitle();
		}
		ShowWindow(hwnd, SW_RESTORE);
		SendCommand(hwnd, CMD_SIZERESTORE);
		break;
		
	// View menu:
		
	case CMD_GOWINDOWED:
		bComingFromFullscreen = IsMaximized();
		if (bComingFromFullscreen) {
			// Restore window from full-screen, don't use ShowRestored() since that do an unsuitable window animation, SetWindowPlacement() doesn't
			UpdateCaption(false);
			WINDOWPLACEMENT wpl;
			wpl.length = sizeof(wpl);
			if (GetWindowPlacement(hwnd, &wpl)) {
				wpl.showCmd = SW_SHOWNORMAL;
				if (SetWindowPlacement(hwnd, &wpl)) {
					return;
				}
			}
		}
		else {
			ShowRestored();
		}
		break;

	case CMD_GOFULLSCREEN:
		ShowMaximized();
		break;

	case CMD_FULLSCREENTOGGLE:
		PostCommand(hwnd, IsMaximized() ? CMD_GOWINDOWED : CMD_GOFULLSCREEN);
		break;
		
	case CMD_TOPMOSTTOGGLE:
		WindowType.bIsTopMost = !WindowType.bIsTopMost;
		SetForegroundWindow(hwnd);
		SetTopMost(WindowType.bIsTopMost);
		UpdateWorkArea();
		break;
		
	case CMD_FREEZE:
		if (iInhibitCaptionChange) {
			PostCommand(hwnd, wId, wCode);	// Delay until later (e.g. if context menu has not yet been removed)
			break;
		}
		UpdateCaption(false);
		bFrozen = true;
		SetAccelerators(FROZEN_MENU);
		break;
		
	case CMD_ONSHOWHIDEWAITCURSOR:
		nShowWaitCursor += (wCode != 0) ? 1 : -1;
		OnSetCursor();
		break;

	case CMD_ONRUNNINGSLIDESHOW:
		bSlideShow = wCode != 0;
		UpdateTitle();
		break;

	case CMD_ONNEWIMAGE:
		OnNewImage();
		break;

	case CMD_BRINGTOFOREGROUND:
		BringWindowToForeground();
		UpdateWindow();	// Req. for some Window XP installs for some reason, or it'll be "invisibly on top"
		break;
		
	case CMD_SIZE2IMAGE:
		iInhibitCaptionChange++;
		Size2Image();
		iInhibitCaptionChange--;
		break;
		
	case CMD_SIZE2WINDOW: 
		iInhibitCaptionChange++;
		Size2Window(true);
		iInhibitCaptionChange--;
		break;
		
	case CMD_TRIM2WINDOW:
		iInhibitCaptionChange++;
		Trim2Window();
		iInhibitCaptionChange--;
		break;
		
	case CMD_SIZERESTORE: 
		iInhibitCaptionChange++;
		Size2Original();
		iInhibitCaptionChange--;
		break;
		
	case CMD_ADJUSTIMAGE:
		iInhibitCaptionChange++;
		bPreventDrawToDesktop = true;

		ShowImageAdjustmentDialog(hwnd, pimd);

		bPreventDrawToDesktop = false;
		iInhibitCaptionChange--;
		break;
		
	case CMD_ROTATE_90:
	case CMD_ROTATE_180:
	case CMD_ROTATE_270:
	case CMD_MIRROR_VERT:
	case CMD_MIRROR_HORIZ: {
		iInhibitCaptionChange++;
		bool bMaxd = pCfg->vwr.bAutoShrinkInFullscreen && IsMaximized() && ((pimd->DispImage()->X() == whClient.w) || (pimd->DispImage()->Y() == whClient.h));
		ieOrientation cmd = pimd->Adjustments()->GetRotation();
		switch (wId) {
	    case CMD_ROTATE_90:
			cmd = ieAddRotation(cmd, ieOrientation::Rotate90);
			break;
		case CMD_ROTATE_180:
			cmd = ieAddRotation(cmd, ieOrientation::Rotate180);
			break;
		case CMD_ROTATE_270:
			cmd = ieAddRotation(cmd, ieOrientation::Rotate270);
			break;
		case CMD_MIRROR_VERT:
			cmd = ieMirrorVert(cmd);
	    	break;
		case CMD_MIRROR_HORIZ:
			cmd = ieMirrorHoriz(cmd);
	    	break;
		}

		SendMessage(hwnd, WM_COMMAND, CMD_ONSHOWHIDEWAITCURSOR | (1 << 16), 0);

		pimd->Adjustments()->SetRotation(cmd);
		pimd->AdjustmentsHasChanged();

		SendMessage(hwnd, WM_COMMAND, CMD_ONSHOWHIDEWAITCURSOR, 0);

		if (bMaxd) Size2Original();

		OnNewImage();

		iInhibitCaptionChange--;
	}   break;
		
	case CMD_ZOOM_IN:
	case CMD_ZOOM_OUT:
	case CMD_ZOOM10:
	case CMD_ZOOM25:
	case CMD_ZOOM50:
	case CMD_ZOOM75:
	case CMD_ZOOM100:
	case CMD_ZOOM150:
	case CMD_ZOOM200:
	case CMD_ZOOM300:
	case CMD_ZOOM400: 
	case CMD_ZOOM_PRINTPREVIEW:
	{
		iInhibitCaptionChange++;
		
		double dZoomFactor = pimd->Adjustments()->GetZoom();

		switch (wId) {
		case CMD_ZOOM_IN:	dZoomFactor *= 1.1892071150027210667;	break;	//1.189... = 2^0.25
		case CMD_ZOOM_OUT:	dZoomFactor /= 1.1892071150027210667;	break; // i.e. zoom 4 times for factor 2x
		case CMD_ZOOM10:	dZoomFactor = 0.10;			break;
		case CMD_ZOOM25:	dZoomFactor = 0.25;			break;
		case CMD_ZOOM50:	dZoomFactor = 0.50;			break;
		case CMD_ZOOM75:	dZoomFactor = 0.75;			break;
		case CMD_ZOOM100:	dZoomFactor = 1.00;			break;
		case CMD_ZOOM150:	dZoomFactor = 1.50;			break;
		case CMD_ZOOM200:	dZoomFactor = 2.00;			break;
		case CMD_ZOOM300:	dZoomFactor = 3.00;			break;
		case CMD_ZOOM400:	dZoomFactor = 4.00;			break;
		case CMD_ZOOM_PRINTPREVIEW:
			if (pimd->Text()) {
				iePixelDensity pd = { 0, 0 };
				ie_ParsePixelDensityInfoStr(pd, pimd->Text()->Get(ieTextInfoType::PixelDensity), iePixelsPerInch);
				if (pd.dwXpU) {
					dZoomFactor = double(muiGetMonitorDpi(hwnd)) / double(pd.dwXpU);
				}
			}	
			break;
		}

		ZoomImage(dZoomFactor);

		iInhibitCaptionChange--;
	}	break;
		
	case CMD_SETWALLPAPER:
		SetDesktopWallpaper(hwnd, pimd);
		break;
		
	case CMD_SENDTOBACK:
		SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
		break;

	case CMD_CLRPICK: {

		muiCoord xyCur;
		muiGetCursorPos(xyCur);
		ScreenToClient(xyCur);

		muiSize whWin;
		GetSizeOfWindow(whWin);
		if ((xyCur.x < 0) || (xyCur.y < 0) || (xyCur.x >= whWin.w) || (xyCur.y >= whWin.h))
			 break;

		HDC hdc = GetDC(hwnd);

		COLORREF cr = GetPixel(hdc, xyCur.x, xyCur.y);

		ReleaseDC(hwnd, hdc);

		TCHAR sz[64];
		_stprintf(sz, _T("     R:\t%d\n     G:\t%d\n     B:\t%d"), GetRValue(cr), GetGValue(cr), GetBValue(cr));

		iInhibitCaptionChange++;
		MessageBox(hwnd, sz, _T("RGB"), MB_OK | MB_TOPMOST);

		iInhibitCaptionChange--;
	}	break;
	}
}


static bool cbOnCreateFileMenu(void *pContext, muiPopupMenu *pMenu)
{
	return ((ViewerWnd *)pContext)->OnCreateFileMenu(pMenu);
}

static bool cbOnCreateViewMenu(void *pContext, muiPopupMenu *pMenu)
{
	return ((ViewerWnd *)pContext)->OnCreateViewMenu(pMenu);
}

static bool cbOnCreateViewZoomMenu(void *pContext, muiPopupMenu *pMenu)
{
	return ((ViewerWnd *)pContext)->OnCreateViewZoomMenu(pMenu);
}

static bool cbOnCreateSlideshowMenu(void *pContext, muiPopupMenu *pMenu)
{
	return ((ViewerWnd *)pContext)->OnCreateSlideshowMenu(pMenu);
}


void ViewerWnd::OnContextMenu(muiCoord xyPosScr, int nButtonThatLaunchedMenu)
{
	if (bPreventContextMenu) return;

    // Create right mouse button (context) menu
	pMenu = new muiPopupMenu();
	if (!pMenu) return;

	iInhibitCaptionChange++;
	bPreventDrawToDesktop = true;

	pMenu->Create(hwnd, xyPosScr, { 1, 1 }, &ft, nButtonThatLaunchedMenu);
	
	pMenu->AddHeader(IEYEICO, NULL, 0, NULL, LOGOICO);

	if (bSlideShow) {
		pMenu->AddCommand(SLIDEICO, ieTranslate(_T("Cancel")), MUI_COLOR_BLACK, hwnd, CMD_SLIDECANCEL, 30271, true, true);
		pMenu->AddCommand(0, ieTranslate(_T("Slideshow")), MUI_COLOR_BLACK, hwnd, 0, 0, false);
		pMenu->AddSeparator();
		pMenu->AddCommand(EXITICO, ieTranslate(_T("Close")), MUI_COLOR_BLACK, hwnd, CMD_SLIDECANCEL, 30272);
	}
	else if (bFrozen) {

		if (!pFrozenKeyAccels) pFrozenKeyAccels = new muiKeyAccelerators(FROZEN_MENU);
		pMenu->MapStatusTextsFromKeyAccel(pFrozenKeyAccels);

		pMenu->AddCommand(CHECKMARKICO, ieTranslate(_T("Freeze window")), MUI_COLOR_BLACK, hwnd, CMD_FREEZE);
		pMenu->AddCommand(NEWICO, ieTranslate(_T("Spawn new...")), MUI_COLOR_BLACK, hwnd, CMD_SPAWNNEW);
		pMenu->AddSeparator();

		pMenu->AddCommand(OPENICO, ieTranslate(_T("Load new...")), MUI_COLOR_BLACK, hwnd, CMD_LOADNEW);
		pMenu->AddCommand(NEXTICO, ieTranslate(_T("Next image file")), MUI_COLOR_BLACK, hwnd, CMD_NEXTIMAGE);
		pMenu->AddCommand(PREVICO, ieTranslate(_T("Previous image file")), MUI_COLOR_BLACK, hwnd, CMD_PREVIMAGE);
		pMenu->AddSeparator();

		pMenu->AddCommand(ZOOMINICO, ieTranslate(_T("Zoom in")), MUI_COLOR_BLACK, hwnd, CMD_ZOOM_IN);
		pMenu->AddCommand(ZOOMOUTICO, ieTranslate(_T("Zoom out")), MUI_COLOR_BLACK, hwnd, CMD_ZOOM_OUT);
		pMenu->AddSeparator();

		pMenu->AddCommand(HELPICO, ieTranslate(_T("Help")), MUI_COLOR_BLACK, hwnd, CMD_HELP);
		pMenu->AddCommand(EXITICO, ieTranslate(_T("Close")), MUI_COLOR_BLACK, hwnd, CMD_EXIT);
		
	} 
	else {
	
		if (!pViewerKeyAccels) pViewerKeyAccels = new muiKeyAccelerators(VIEWER_MENU);
		pMenu->MapStatusTextsFromKeyAccel(pViewerKeyAccels);

		if (IsMaximized()) pMenu->AddCommand(0, ieTranslate(_T("Restore window")), MUI_COLOR_BLACK, hwnd, CMD_GOWINDOWED);
		pMenu->AddCommand(OPENICO, ieTranslate(_T("Load new...")), MUI_COLOR_BLACK, hwnd, CMD_LOADNEW);
		pMenu->AddCommand(NEWICO, ieTranslate(_T("Spawn new...")), MUI_COLOR_BLACK, hwnd, CMD_SPAWNNEW);
		pMenu->AddSeparator();

		pMenu->AddCommand(NEXTICO, ieTranslate(_T("Next image file")), MUI_COLOR_BLACK, hwnd, CMD_NEXTIMAGE);
		pMenu->AddCommand(PREVICO, ieTranslate(_T("Previous image file")), MUI_COLOR_BLACK, hwnd, CMD_PREVIMAGE);
		pMenu->AddSeparator();

		pMenu->AddSubMenu(0, ieTranslate(_T("File")), MUI_COLOR_BLACK, cbOnCreateFileMenu, this);
		pMenu->AddSubMenu(0, ieTranslate(_T("View")), MUI_COLOR_BLACK, cbOnCreateViewMenu, this);
		pMenu->AddSubMenu(0, ieTranslate(_T("Slideshow")), MUI_COLOR_BLACK, cbOnCreateSlideshowMenu, this);
		pMenu->AddCommand(INDEXICO, ieTranslate(_T("Index")), MUI_COLOR_BLACK, hwnd, CMD_INDEX);
		pMenu->AddSeparator();

		pMenu->AddCommand(CLRICO, ieTranslate(_T("Adjust image...")), MUI_COLOR_BLACK, hwnd, CMD_ADJUSTIMAGE);
		pMenu->AddCommand(OPTIONSICO, ieTranslate(_T("Options")), MUI_COLOR_BLACK, hwnd, CMD_PROPERTIES);
		pMenu->AddSeparator();

		pMenu->AddCommand(HELPICO, ieTranslate(_T("Help")), MUI_COLOR_BLACK, hwnd, CMD_HELP);
		pMenu->AddCommand(EXITICO, ieTranslate(_T("Close")), MUI_COLOR_BLACK, hwnd, CMD_EXIT);
	}

	pMenu->TrackPopup();

	pMenu = nullptr;
	bPreventDrawToDesktop = false;
	iInhibitCaptionChange--;
}


bool ViewerWnd::OnCreateFileMenu(muiPopupMenu *pMenu)
{
	pMenu->MapStatusTextsFromKeyAccel(pViewerKeyAccels);

	pMenu->AddSeparator(true);

	pMenu->AddCommand(OPENICO, ieTranslate(_T("Load new...")), MUI_COLOR_BLACK, hwnd, CMD_LOADNEW);
	pMenu->AddCommand(NEXTICO, ieTranslate(_T("Next image file")), MUI_COLOR_BLACK, hwnd, CMD_NEXTIMAGE);
	pMenu->AddCommand(PREVICO, ieTranslate(_T("Previous image file")), MUI_COLOR_BLACK, hwnd, CMD_PREVIMAGE);
	pMenu->AddCommand(0, ieTranslate(_T("First image file in directory")), MUI_COLOR_BLACK, hwnd, CMD_FIRSTIMAGE);
	pMenu->AddCommand(0, ieTranslate(_T("Last image file in directory")), MUI_COLOR_BLACK, hwnd, CMD_LASTIMAGE);
	pMenu->AddCommand(CLOSEALLICO, ieTranslate(_T("Close all image windows")), MUI_COLOR_BLACK, hwnd, CMD_KILLEMALL);
	pMenu->AddSeparator();

	pMenu->AddCommand(NEWICO, ieTranslate(_T("Spawn new...")), MUI_COLOR_BLACK, hwnd, CMD_SPAWNNEW);
	pMenu->AddCommand(0, ieTranslate(_T("Spawn next image")), MUI_COLOR_BLACK, hwnd, CMD_SPAWNNEXTIMAGE);
	pMenu->AddCommand(0, ieTranslate(_T("Spawn previous image")), MUI_COLOR_BLACK, hwnd, CMD_SPAWNPREVIMAGE);
	pMenu->AddSeparator();

	pMenu->AddCommand(FILEINFOICO, ieTranslate(_T("Image file information...")), MUI_COLOR_BLACK, hwnd, CMD_FILEINFO);
	pMenu->AddCommand(SAVEICO, ieTranslate(_T("Save as...")), MUI_COLOR_BLACK, hwnd, CMD_SAVEAS);

	bool bGotSrcFile = pimd->Text()->Have(ieTextInfoType::SourceFile);
	if (bGotSrcFile) pMenu->AddCommand(RELOADICO, ieTranslate(_T("Reload from file")), MUI_COLOR_BLACK, hwnd, CMD_RELOAD);
	if (bGotSrcFile) {	
		pMenu->AddSeparator();

		pMenu->AddCommand(0, ieTranslate(_T("Copy file...")), MUI_COLOR_BLACK, hwnd, CMD_COPY);
		pMenu->AddCommand(0, ieTranslate(_T("Move file...")), MUI_COLOR_BLACK, hwnd, CMD_MOVE);
		pMenu->AddCommand(0, ieTranslate(_T("Rename file...")), MUI_COLOR_BLACK, hwnd, CMD_RENAME);
		pMenu->AddCommand(0, ieTranslate(_T("Delete file...")), MUI_COLOR_BLACK, hwnd, CMD_DELETE);
		pMenu->AddCommand(0, ieTranslate(_T("Delete this & show next...")), MUI_COLOR_BLACK, hwnd, CMD_DELETE_SHOWNEXT);
		pMenu->AddCommand(0, ieTranslate(_T("Erase this & show next...")), MUI_COLOR_BLACK, hwnd, CMD_ERASE_SHOWNEXT);
	}
	pMenu->AddSeparator();

	pMenu->AddCommand(0, ieTranslate(_T("Copy to clipboard")), MUI_COLOR_BLACK, hwnd, CMD_CLIPBCOPY);
	pMenu->AddCommand(0, ieTranslate(_T("Paste from clipboard")), MUI_COLOR_BLACK, hwnd, CMD_CLIPBPASTE, 0, Clipboard_Query());
	pMenu->AddCommand(0, ieTranslate(_T("Capture a screen shot")), MUI_COLOR_BLACK, hwnd, CMD_CAPTURESCREEN);
	pMenu->AddCommand(0, ieTranslate(_T("Capture a window shot")), MUI_COLOR_BLACK, hwnd, CMD_CAPTUREWINDOW);

	pMenu->AddSeparator(true);

	return true;
}


bool ViewerWnd::OnCreateViewMenu(muiPopupMenu *pMenu)
{
	bool bIsFullscreen = IsMaximized();

	pMenu->MapStatusTextsFromKeyAccel(pViewerKeyAccels);

	pMenu->AddSeparator(true);

	TCHAR sz[64];
	*sz = 0;
	if (pViewerKeyAccels) pViewerKeyAccels->Command2KeyStr(sz, (sizeof(sz)/sizeof(TCHAR))-1, CMD_FULLSCREENTOGGLE);
	if (bIsFullscreen) {
		pMenu->AddCommand(WINDOWEDICO, ieTranslate(_T("Restore window")), MUI_COLOR_BLACK, hwnd, CMD_GOWINDOWED, 0, true, false, sz);
	} else {
		pMenu->AddCommand(FULLSCREENICO, ieTranslate(_T("Fullscreen mode")), MUI_COLOR_BLACK, hwnd, CMD_GOFULLSCREEN, 0, true, false, sz);
	}
	if ((pimd->DispX() != whClient.w) || (pimd->DispY() != whClient.h)) {
		pMenu->AddCommand(SIZE2IMGICO, ieTranslate(_T("Size window to image")), MUI_COLOR_BLACK, hwnd, CMD_SIZE2IMAGE);
		pMenu->AddCommand(SIZE2WINICO, ieTranslate(_T("Size image to window")), MUI_COLOR_BLACK, hwnd, CMD_SIZE2WINDOW);
	}
	if ((pimd->X() != pimd->DispX()) || (pimd->Y() != pimd->DispY())) {
		pMenu->AddCommand(RESTOREICO, ieTranslate(_T("Restore original size")), MUI_COLOR_BLACK, hwnd, CMD_SIZERESTORE);
	}
	if ((pimd->DispX() > whClient.w) || (pimd->DispY() > whClient.h)) {
		pMenu->AddCommand(0, ieTranslate(_T("Trim image to window")), MUI_COLOR_BLACK, hwnd, CMD_TRIM2WINDOW);
	}
	pMenu->AddCommand(WindowType.bIsTopMost ? CHECKMARKICO : 0, ieTranslate(_T("Topmost window")), MUI_COLOR_BLACK, hwnd, CMD_TOPMOSTTOGGLE);
	if (!bIsFullscreen) pMenu->AddCommand(0, ieTranslate(_T("Freeze window")), MUI_COLOR_BLACK, hwnd, CMD_FREEZE);
	pMenu->AddSeparator();

	pMenu->AddCommand(CLRICO, ieTranslate(_T("Adjust image...")), MUI_COLOR_BLACK, hwnd, CMD_ADJUSTIMAGE);
	pMenu->AddSeparator();

	ieOrientation eRot = pimd->Adjustments()->GetRotation();
	pMenu->AddCommand(ROTRIGHTICO,	ieTranslate(_T("Rotate image +90°")), MUI_COLOR_BLACK, hwnd, CMD_ROTATE_270);
	pMenu->AddCommand(ROTLEFTICO, ieTranslate(_T("Rotate image -90°")), MUI_COLOR_BLACK, hwnd, CMD_ROTATE_90);
	pMenu->AddCommand(0, ieTranslate(_T("Rotate image 180°")), MUI_COLOR_BLACK, hwnd, CMD_ROTATE_180);
	pMenu->AddCommand(ieIsMirrorVert( eRot) ? CHECKMARKICO : 0, ieTranslate(_T("Mirror image vertically")), MUI_COLOR_BLACK, hwnd, CMD_MIRROR_VERT);
	pMenu->AddCommand(ieIsMirrorHoriz(eRot) ? CHECKMARKICO : 0, ieTranslate(_T("Mirror image horizontally")), MUI_COLOR_BLACK, hwnd, CMD_MIRROR_HORIZ);
	pMenu->AddSeparator();

	pMenu->AddCommand(ZOOMINICO, ieTranslate(_T("Zoom in")), MUI_COLOR_BLACK, hwnd, CMD_ZOOM_IN);
	pMenu->AddCommand(ZOOMOUTICO, ieTranslate(_T("Zoom out")), MUI_COLOR_BLACK, hwnd, CMD_ZOOM_OUT);
	pMenu->AddSubMenu(0, ieTranslate(_T("Zoom to factor")), MUI_COLOR_BLACK, cbOnCreateViewZoomMenu, this);
	pMenu->AddSeparator();

	pMenu->AddCommand(0, ieTranslate(_T("Use as desktop wallpaper")), MUI_COLOR_BLACK, hwnd, CMD_SETWALLPAPER);
	pMenu->AddCommand(0, ieTranslate(_T("Send window to back")), MUI_COLOR_BLACK, hwnd, CMD_SENDTOBACK);

	pMenu->AddSeparator(true);

	return true;
}


bool ViewerWnd::OnCreateViewZoomMenu(muiPopupMenu *pMenu)
{
	pMenu->MapStatusTextsFromKeyAccel(pViewerKeyAccels);

	int iZoom = int(pimd->Adjustments()->GetZoom()*100.0f + 0.5f);

	pMenu->AddSeparator(true);

	pMenu->AddCommand((iZoom ==  10) ? CHECKMARKICO : 0,  ieTranslate(_T("Zoom to 10% size")), MUI_COLOR_BLACK, hwnd,  CMD_ZOOM10);
	pMenu->AddCommand((iZoom ==  25) ? CHECKMARKICO : 0,  ieTranslate(_T("Zoom to 25% size")), MUI_COLOR_BLACK, hwnd,  CMD_ZOOM25);
	pMenu->AddCommand((iZoom ==  50) ? CHECKMARKICO : 0,  ieTranslate(_T("Zoom to 50% size")), MUI_COLOR_BLACK, hwnd,  CMD_ZOOM50);
	pMenu->AddCommand((iZoom ==  75) ? CHECKMARKICO : 0,  ieTranslate(_T("Zoom to 75% size")), MUI_COLOR_BLACK, hwnd,  CMD_ZOOM75);
	pMenu->AddCommand((iZoom == 100) ? CHECKMARKICO : 0, ieTranslate(_T("Zoom to 100% size")), MUI_COLOR_BLACK, hwnd, CMD_ZOOM100);
	pMenu->AddCommand((iZoom == 150) ? CHECKMARKICO : 0, ieTranslate(_T("Zoom to 150% size")), MUI_COLOR_BLACK, hwnd, CMD_ZOOM150);
	pMenu->AddCommand((iZoom == 200) ? CHECKMARKICO : 0, ieTranslate(_T("Zoom to 200% size")), MUI_COLOR_BLACK, hwnd, CMD_ZOOM200);
	pMenu->AddCommand((iZoom == 300) ? CHECKMARKICO : 0, ieTranslate(_T("Zoom to 300% size")), MUI_COLOR_BLACK, hwnd, CMD_ZOOM300);
	pMenu->AddCommand((iZoom == 400) ? CHECKMARKICO : 0, ieTranslate(_T("Zoom to 400% size")), MUI_COLOR_BLACK, hwnd, CMD_ZOOM400);

	pMenu->AddSeparator(true);

	return true;
}


bool ViewerWnd::OnCreateSlideshowMenu(muiPopupMenu *pMenu)
{
	pMenu->MapStatusTextsFromKeyAccel(pViewerKeyAccels);

	pMenu->AddSeparator(true);

	pMenu->AddCommand(SLIDEICO, ieTranslate(_T("Slideshow")), MUI_COLOR_BLACK, hwnd, CMD_SLIDEAUTO);
	pMenu->AddCommand(OPTIONSICO, ieTranslate(_T("Create slideshow script")), MUI_COLOR_BLACK, hwnd, CMD_SLIDESHOW);
	pMenu->AddCommand(pCfg->sli.bRunMaximized ? CHECKMARKICO : 0, ieTranslate(_T("Run in fullscreen mode")), MUI_COLOR_BLACK, hwnd, CMD_SLIDEFULLSCREEN);

	pMenu->AddSeparator(true);

	return true;
}


void ViewerWnd::OnDestroy()
{
	iInhibitCaptionChange++;
	
	KillTimer(hwnd, 1);
	if (bAutoCaptionTimer) KillTimer(hwnd, 42);
	
	ShowWindow(hwnd, SW_HIDE);			// NB, must do this due to Win 9X/NT bug when the Windows style has been changed, else the app wont be removed from the task bar - see Ms knowledge base articile 214655

	muiMultiWindowedAppWindow::OnDestroy();
}


void ViewerWnd::OnDropFiles(HANDLE hDrop)
{
    // If user drops a file in the window, spawn a new window with it
	iInhibitCaptionChange++;
	
	TCHAR	s[MAX_PATH];
	int		iNoFiles = DragQueryFile((HDROP)hDrop, 0xFFFFFFFF, NULL, 0);

	for (int i = 0; i < iNoFiles; i++) {

		DragQueryFile((HDROP)hDrop, i, s, sizeof(s)/sizeof(TCHAR));

		if (!i) LoadNewImage(s, true, true);
		else SpawnViewer(s);
	}

	DragFinish((HDROP)hDrop);
	
	iInhibitCaptionChange--;
}


void ViewerWnd::OnKey(int nVirtKey, bool bKeyDown, bool bKeyRepeat, bool bAltDown)
{
	if (bSlideShow) return;

	if (pMenu && pMenu->NavigateOnKey(nVirtKey, bKeyDown))
		return;

	muiAppWindow::OnKey(nVirtKey, bKeyDown, bKeyRepeat, bAltDown);

	if (!pimd) return;

	if (bKeyDown) {
		// Handle keypresses (arrows=scroll, ins,home,del,end=corners, esc=quit):
		muiCoord xyDelta(0, 0);
		bool bCenter = false;
		
		switch (nVirtKey) {

		case VK_CONTROL:
			bCtrlDown = true;
			if (bKeyRepeat) return;
			break;

		case VK_SHIFT:
			bShiftDown = true;
			if (bKeyRepeat) return;
			break;

		case VK_MENU:
			ViewerWnd::bAltDown = true;
			if (bKeyRepeat) return;
			OnSetCursor(hwnd, HTCLIENT, WM_MOUSEMOVE);
			break;

		case VK_ESCAPE:
			SendMessage(hwnd, WM_COMMAND, CMD_EXIT, 0);
			break;

		case VK_NUMPAD4:
			xyDelta.x = bShiftDown ? -100 : -10;
			break;
		case VK_NUMPAD6:
			xyDelta.x = bShiftDown ?  100 :  10;
			break;
		case VK_NUMPAD8:
			xyDelta.y = bShiftDown ? -100 : -10;
			break;
		case VK_NUMPAD2:
			xyDelta.y = bShiftDown ?  100 :  10;
			break;
		case VK_NUMPAD7:
			xyDelta.x = bShiftDown ? -100 : -10;
			xyDelta.y = bShiftDown ? -100 : -10;
			break;
		case VK_NUMPAD9:
			xyDelta.x = bShiftDown ?  100 :  10;
			xyDelta.y = bShiftDown ? -100 : -10;
			break;
		case VK_NUMPAD1:
			xyDelta.x = bShiftDown ? -100 : -10;
			xyDelta.y = bShiftDown ?  100 :  10;
			break;
		case VK_NUMPAD3:
			xyDelta.x = bShiftDown ?  100 :  10;
			xyDelta.y = bShiftDown ?  100 :  10;
			break;
		case VK_NUMPAD5:
			bCenter = true;
			break;
    	}

		if (xyDelta.x || xyDelta.y || bCenter) {

			bool bPanX = (pimd->DispX() > whClient.w);
			bool bPanY = (pimd->DispY() > whClient.h);
			bool bPanningCursor = (bPanX || bPanY) && !bAltDown && !bMidDown && !bFrozen;

			if (bPanningCursor || bFrozen) {

				if (bCenter) {
					xyPan = { ieCoord((pimd->DispX() - whClient.w) / 2), ieCoord((pimd->DispY() - whClient.h) / 2) };
				}

				xyPan.nX += xyDelta.x;
				xyPan.nY += xyDelta.y;

				Repaint();

			} else {

				muiCoord xyPos;
				if (bCenter) {
					xyPos = xyWorkOrigo;
					xyPos.x += (whWorkArea.w - whClient.w) >> 1;
					xyPos.y += (whWorkArea.h - whClient.h) >> 1;
				} else {
					xyPos = { 0, 0 };
					ClientToScreen(xyPos);
					xyPos += xyDelta;
				}

				if (xyPos.x + whClient.w > xyWorkOrigo.x + whWorkArea.w) xyPos.x = xyWorkOrigo.x + whWorkArea.w - whClient.w;
				if (xyPos.x < xyWorkOrigo.x) xyPos.x = xyWorkOrigo.x;

				if (xyPos.y + whClient.h > xyWorkOrigo.y + whWorkArea.h) xyPos.y = xyWorkOrigo.y + whWorkArea.h - whClient.h;
				if (xyPos.y < xyWorkOrigo.y) xyPos.y = xyWorkOrigo.y;

				UpdateWindow(&xyPos, NULL, false);
			}
		}
	
	} else { // bUp!

		switch (nVirtKey) {

		case VK_CONTROL:
			bCtrlDown = false;
			break;

		case VK_SHIFT:
			bShiftDown = false;
			break;

		case VK_MENU:
			ViewerWnd::bAltDown = false;
			if (!bFrozen) OnSetCursor(hwnd, HTCLIENT, WM_MOUSEMOVE);
			break;
		}
	}
}


bool ViewerWnd::OnSizing(WPARAM fwWhichEdge, RECT *prcDragRect)
{ 
	if (!pCfg->vwr.bAutoSizeImage || bInhibitAutoSize || !pimd) return false;

	if (pCfg->vwr.bAutoSizeOnlyShrink && (whClient.w > pimd->X()) && (whClient.h > pimd->Y())) return false;

	muiCoord xyCurPos;
	GetScreenPosOfWindow(xyCurPos);

	const muiSize whNca{ 2 * ciFrameXW, 2 * ciFrameYH + ciCaptionYH };
	const muiCoord xyMinPos{ min(xyWorkOrigo.x - (ciFrameXW), xyCurPos.x), min(xyWorkOrigo.y - (ciFrameYH + ciCaptionYH), xyCurPos.y) };
	const muiCoord xyMaxPos{ max(xyWorkOrigo.x + whWorkArea.w + ciFrameXW, xyCurPos.x + whClient.w + whNca.w), max(xyWorkOrigo.y + whWorkArea.h + ciFrameYH, xyCurPos.y + whClient.h + whNca.h) };

	int iXW = (prcDragRect->right - prcDragRect->left) - whNca.w;
	int iYH = (prcDragRect->bottom - prcDragRect->top) - whNca.h;

	switch (fwWhichEdge) {
	case WMSZ_LEFT:
	case WMSZ_RIGHT:
		iYH = int(iXW*double(pimd->Y())/double(pimd->X())+0.5);
		break;
	case WMSZ_TOP:
	case WMSZ_BOTTOM:
		iXW = int(iYH*double(pimd->X())/double(pimd->Y())+0.5);
		break;
	}

	if (iXW+whNca.w <= ciMinWinXW) {
		iXW = ciMinWinXW - whNca.w;
	}

	switch (fwWhichEdge) {
	case WMSZ_TOPLEFT:
	case WMSZ_BOTTOMLEFT:
		prcDragRect->left = prcDragRect->right - (iXW + whNca.w);
		break;
	case WMSZ_TOP:
	case WMSZ_BOTTOM:
		prcDragRect->left = (prcDragRect->left + prcDragRect->right) / 2;
		prcDragRect->left -= (iXW + whNca.w) / 2;
		if (prcDragRect->left + (iXW + whNca.w) > xyMaxPos.x)
			prcDragRect->left = xyMaxPos.x - (iXW + whNca.w);
		if (prcDragRect->left < xyMinPos.x)
			prcDragRect->left = -xyMinPos.x;
		prcDragRect->right = prcDragRect->left + (iXW + whNca.w);
		break;
	default:
		prcDragRect->right = prcDragRect->left + (iXW + whNca.w);
		break;
	}

	switch (fwWhichEdge) {
	case WMSZ_TOPLEFT:
	case WMSZ_TOPRIGHT:
		prcDragRect->top = prcDragRect->bottom - (iYH + whNca.h);
		break;
	case WMSZ_LEFT:
	case WMSZ_RIGHT:
		prcDragRect->top = (prcDragRect->top + prcDragRect->bottom) / 2;
		prcDragRect->top -= (iYH + whNca.h) / 2;
		if (prcDragRect->top + (iYH + whNca.h) > xyMaxPos.y)
			prcDragRect->top = xyMaxPos.y - (iYH + whNca.h);
		if (prcDragRect->top < xyMinPos.y) 
			prcDragRect->top = xyMinPos.y;
		prcDragRect->bottom = prcDragRect->top + (iYH + whNca.h);
		break;
	default:
		prcDragRect->bottom = prcDragRect->top + (iYH + whNca.h);
		break;
	}

	return true;
}


bool ViewerWnd::OnPosChanging(WINDOWPOS *pNewState)
{
	if (pNewState->flags & 0x8000) {
		// Undocumented SWP_STATECHANGED
		if (IsMaximized()) {
			// We're about to go to maximized, only clear screen first, repaint with image after maximize is done
			UpdateWorkArea();
			whClient = whWorkArea;
			bPaintBackgroundOnly = true;		
		}
		//else if (!IsMinimized()) {
			// We're about to go to normal (from maximized or from iconic)
		//}
	}

	return false;
}


void ViewerWnd::OnSize(WPARAM fwSizeType, muiSize whNewSize)
{
	if (!pimd || iInhibitSizeChange) return;
	
	switch (fwSizeType) {
	
	case SIZE_RESTORED: {

		if (!WindowType.bIsTopMost)	SetTopMost(false);

		UpdateWorkArea();

		if (!WindowType.bLayered) {
			GetSizeOfWindow(whClient);
		}

		UpdateGlassState(true);
		UpdateCaption(false);

		if (bComingFromFullscreen) {

			bComingFromFullscreen = false;

			if (pCfg->vwr.bAutoSizeImage && !bInhibitAutoSize) {
				if (bDontSize2WindowOnRestore) {
					Size2Original();
					bDontSize2WindowOnRestore = false;
					Size2Window(true);
				} else {
					Size2Window(false);
				}
			} else {
				Size2Image();
			}

		} else if (pCfg->vwr.bAutoSizeImage && !bInhibitAutoSize && ((whClient.w != pimd->DispX()) || (whClient.h != pimd->DispY())) && (!pCfg->vwr.bAutoSizeOnlyShrink || (whClient.w < pimd->X()) || (whClient.h < pimd->Y())))  {

			Size2Window(false, whClient.h >= whWorkArea.h);

		} else {

			UpdateWindow();

		}

	}	break;

	case SIZE_MAXIMIZED: {

		bPaintBackgroundOnly = false;
		SetTopMost(true);
		UpdateGlassState(false);
		UpdateCaption(true);

		if (((!bInhibitAutoSize && pCfg->vwr.bAutoSizeImage && !pCfg->vwr.bAutoSizeOnlyShrink) && ((whClient.w != pimd->X()) || (whClient.h != pimd->Y())))
			|| (((!bInhibitAutoSize && pCfg->vwr.bAutoSizeImage) || pCfg->vwr.bAutoShrinkInFullscreen) && ((whClient.w < pimd->X()) || (whClient.h < pimd->Y())))) {
			Size2Window(false);
		}
	}	break;
	}
}


void ViewerWnd::OnMove(muiCoord xyNewPos)
{
	muiCoord xyPrevWorkOrigo = xyWorkOrigo;
	muiSize whPrevWorkArea = whWorkArea;

	UpdateWorkArea();		// Check if we've moved to a differently sized monitor

	if ((xyWorkOrigo != xyPrevWorkOrigo) || (whWorkArea != whPrevWorkArea)) {
		UpdateSystemMetrics();
		UpdateWindow();
	}
}


inline void GetRectCenter(const RECT &rc, POINT &pt)
{
    pt.x = rc.left + (rc.right - rc.left)/2;
    pt.y = rc.top  + (rc.bottom - rc.top)/2;
}


void ViewerWnd::OnMessage(UINT uMsg, WPARAM wParam, LPARAM lParam, int &iReturnValue)
{
	switch (uMsg) {

	case WM_ENTERSIZEMOVE:
		iInhibitCaptionChange++;
		break;

	case WM_EXITSIZEMOVE:
		iInhibitCaptionChange--;
		break;

	case WM_COPYDATA:
		ReceiveFileInFirstProcess(PCOPYDATASTRUCT(lParam)->dwData, PCOPYDATASTRUCT(lParam)->lpData, PCOPYDATASTRUCT(lParam)->cbData);
		iReturnValue = TRUE;
		break;

	case WM_ENDSESSION:
		if (bFrozen) AddAutoLoad(hwnd, pimd->Text()->Get(ieTextInfoType::SourceFile), pimd->Adjustments()->GetZoom());
		break;

	case WM_GETMINMAXINFO: {
	    // Ensure that maximized state has title bar outside of visible screen
		muiCoord xyDisplay;
		muiSize whDisplay;
		muiGetWorkArea(hwnd, xyDisplay, whDisplay, true);

		LPMINMAXINFO pMMI = (LPMINMAXINFO)lParam;

		pMMI->ptMinTrackSize.x = 2*ciFrameXW;
		pMMI->ptMinTrackSize.y = ciCaptionYH + 2*ciFrameYH;
        pMMI->ptMaxTrackSize = pMMI->ptMinTrackSize;
		pMMI->ptMaxTrackSize.x += whDisplay.w;
        pMMI->ptMaxTrackSize.y += whDisplay.h;

        pMMI->ptMaxPosition.x = xyDisplay.x - (              ciFrameXW);
        pMMI->ptMaxPosition.y = xyDisplay.y - (ciCaptionYH + ciFrameYH);
		pMMI->ptMaxSize = pMMI->ptMaxTrackSize;

		iReturnValue = 0;
	}	break;
	
    case WM_DISPLAYCHANGE:
	case WM_DWMCOMPOSITIONCHANGED: {
		// Video mode or compositioning mode has changed
		UpdateWorkArea();
		UpdateSystemMetrics();
		UpdateWindow();
    }	break;

	}
}

void ViewerWnd::OnMouseButton(int nButton, eMouseButtonAction eAction, muiCoord xyPos, WPARAM fKeys)
{
	muiAppWindow::OnMouseButton(nButton, eAction, xyPos, fKeys);

	if (eAction == eButtonDoubleClick) {
		if (nButton <= 2) {
		   	// On left button double-click, toggle between maximized and windowed
			if (bFrozen) return;
			PostCommand(hwnd, IsMaximized() ? CMD_GOWINDOWED : CMD_GOFULLSCREEN);
		}
		return;
	}
	
	bool bDown;
	switch (eAction) {
	case eButtonDown:
		bDown = true;
		break;
	case eButtonUp:
		bDown = false;
		break;
	default:
		return;
	}

	if ((nButton == 0) || (nButton == 2)) {
		if (bDown) {
			if (bFrozen) return;
			muiGetCursorPos(xyLastMouseDrag);
			SetCapture(hwnd);
			iInhibitCaptionChange++;
			bMouseCaptured = true;
		} else {
			if (!bMouseCaptured) return;
			bMouseCaptured = false;
			ReleaseCapture();
			iInhibitCaptionChange--;
		}
		if (nButton == 2) {
			bMidDown = bDown;
			OnSetCursor(hwnd, 0, 0);
		}
	} else if (nButton == 1) {
		if (bDown) {
			bPreventContextMenu = false;
		}
	} else if (nButton == 3) {
		if (bDown) {
			if (bFrozen) return;
			OnCommand((fKeys & MK_CONTROL) ? CMD_SPAWNNEXTIMAGE : CMD_NEXTIMAGE, 0, 0);
		}
	} else if (nButton == 4) {
		if (bDown) {
			if (bFrozen) return;
			OnCommand((fKeys & MK_CONTROL) ? CMD_SPAWNPREVIMAGE : CMD_PREVIMAGE, 0, 0);
		}
	}

}


void ViewerWnd::OnMouseMove(muiCoord xyPos, WPARAM fKeys)
{
	muiAppWindow::OnMouseMove(xyPos, fKeys);

	if (bFrozen) return;

	// Update caption?
	if (pCfg->vwr.bHideCaption) {

		if (!WindowType.bHasCaption) {
			UpdateCaption(true);
		}

		if (!bAutoCaptionTimer) {
			SetTimer(hwnd, 42, 100, NULL);
			bAutoCaptionTimer = true;
		}
	}

    // Scroll with the mouse
	if (!bMouseCaptured || !pimd) return;

	muiCoord xyCursor;
	muiGetCursorPos(xyCursor);

	muiCoord xyDelta = xyCursor;
	xyDelta -= xyLastMouseDrag;

	xyLastMouseDrag = xyCursor;

	bool bPanX = (pimd->DispX() > whClient.w);
	bool bPanY = (pimd->DispY() > whClient.h);
	bool bPanningCursor = (bPanX || bPanY) && !bAltDown && !bMidDown && !bFrozen;

	if (bPanningCursor && !bAltDown && !bMidDown) {
		// Pan image
		xyPan.nX -= xyDelta.x;
		xyPan.nY -= xyDelta.y;
		UpdateWindow();
	} else if ((xyDelta.x || xyDelta.y) && !IsMaximized()) {
		// Move image around screen
		muiCoord xyPos(0, 0);
		ClientToScreen(xyPos);
		xyPos += xyDelta;
		UpdateWindow(&xyPos, NULL, false);
	}
}


void ViewerWnd::OnMouseWheel(int iDelta, muiCoord xyPosScr, WPARAM fKeys)
{
	if (bFrozen || !pimd) return;
	iMouseWheelDelta += iDelta;
	if (fKeys & (MK_LBUTTON | MK_SHIFT)) {
		if (abs(iMouseWheelDelta) >= WHEEL_DELTA) {
			QueueMessage(WM_COMMAND, (iMouseWheelDelta <= 0) ? CMD_NEXTIMAGE : CMD_PREVIMAGE);
			iMouseWheelDelta = 0;
		}
	} else if (fKeys & (MK_RBUTTON | MK_CONTROL)) {
		if (abs(iMouseWheelDelta) >= WHEEL_DELTA) {
			QueueMessage(WM_COMMAND, (iMouseWheelDelta >= 0) ? CMD_ZOOM_IN: CMD_ZOOM_OUT);
			iMouseWheelDelta = 0;
		}
		bPreventContextMenu = true;
	} else if (fKeys & MK_MBUTTON) {
	} else {
		bool bPanX = (pimd->DispX() > whClient.w);
		bool bPanY = (pimd->DispY() > whClient.h);
		int iSteps = max(1, WHEEL_DELTA / 20);
		if (bPanX || bPanY) {
			bool bVertPan = bPanY;
			while (iMouseWheelDelta >= iSteps) {
				iMouseWheelDelta -= iSteps;
				QueueMessage(WM_KEYDOWN, bVertPan ? VK_NUMPAD8 : VK_NUMPAD6, 0x01000000);
			}
			while (iMouseWheelDelta <= -iSteps) {
				iMouseWheelDelta += iSteps;
				QueueMessage(WM_KEYDOWN, bVertPan ? VK_NUMPAD2 : VK_NUMPAD4, 0x01000000);
			}
		}
	}
}


bool ieClipRect(RECT &rc, const RECT &rcClip)
{
	if (rc.left < rcClip.left) {
		rc.left = rcClip.left;
	}
	if (rc.right > rcClip.right) {
		rc.right = rcClip.right;
	}
	if (rc.left >= rc.right) return false;

	if (rc.top < rcClip.top) {
		rc.top = rcClip.top;
	}
	if (rc.bottom > rcClip.bottom) {
		rc.bottom = rcClip.bottom;
	}
	if (rc.top >= rc.bottom) return false;

	return true;
}


template <class X> void BubbleSort(X *items, int count)
{
	for (int a = 1; a < count; a++) {
		for (int b = count; --b >= a; ) {
			X &b0 = items[b];
			X &b1 = items[b - 1];
			if (b1 > b0) {
				X bt = b1;
				b1 = b0;
				b0 = bt;
			}
		}
	}
}


ieBGRA ie_AutoBgColor(iePCImage pim)
{
	const int nMaxSamples = 5;
	int nSamples;
	ieBGRA aClrs[nMaxSamples];
	int aiOffs[nMaxSamples];

	for (int tries = 0; tries < 2; tries++) {

		if (!tries) {
			nSamples = 0;
			aiOffs[nSamples++] = 0;
			aiOffs[nSamples++] = (pim->X() - 1);
			aiOffs[nSamples++] = (pim->Y() - 1) * pim->Pitch();
			aiOffs[nSamples++] = (pim->Y() - 1) * pim->Pitch() + (pim->X() - 1);
			aiOffs[nSamples++] = (pim->Y() >> 1) * pim->Pitch() + (pim->X() >> 1);
		}
		else {
			nSamples = 0;
			aiOffs[nSamples++] = (pim->Y() >> 2) * pim->Pitch() + (pim->X() >> 1);
			aiOffs[nSamples++] = ((3*pim->Y()) >> 2) * pim->Pitch() + (pim->X() >> 1);
			aiOffs[nSamples++] = (pim->Y() >> 1) * pim->Pitch() + (pim->X() >> 2);
			aiOffs[nSamples++] = (pim->Y() >> 1) * pim->Pitch() + ((3*pim->X()) >> 2);
			aiOffs[nSamples++] = (pim->Y() >> 1) * pim->Pitch() + (pim->X() >> 1);
		}

		switch (pim->PixelFormat()) {

		case iePixelFormat::BGRA: {
			iePCImage_BGRA p = pim->BGRA();
			iePCBGRA p4 = p->PixelPtr();
			for (int n = 0; n < nSamples; n++)
				aClrs[n] = p4[aiOffs[n]];
		}	break;

		case iePixelFormat::L: {
			iePCImage_L p = pim->L();
			iePCL p1 = p->PixelPtr();
			for (int n = 0; n < nSamples; n++) {
				aClrs[n].B = p1[aiOffs[n]];
				aClrs[n].G = aClrs[n].B;
				aClrs[n].R = aClrs[n].B;
				aClrs[n].A = 0xFF;
			}
		}	break;

		case iePixelFormat::CLUT: {
			iePCImage_CLUT p = pim->CLUT();
			iePCILUT pI = p->PixelPtr();
			iePCBGRA pCLUT = p->CLUTPtr();
			for (int n = 0; n < nSamples; n++)
				aClrs[n] = pCLUT[pI[n]];
		}	break;

		case iePixelFormat::wBGRA: {
			iePCImage_wBGRA p = pim->wBGRA();
			iePCwBGRA p4 = p->PixelPtr();
			for (int n = 0; n < nSamples; n++)
				ie_wBGRA2BGRA(&aClrs[n], &p4[aiOffs[n]], 1);
		}	break;

		case iePixelFormat::wL: {
			iePCImage_wL p = pim->wL();
			iePCwL p1 = p->PixelPtr();
			for (int n = 0; n < nSamples; n++) {
				ie_wL2L(&aClrs[n].B, &p1[aiOffs[n]], 1);
				aClrs[n].G = aClrs[n].B;
				aClrs[n].R = aClrs[n].B;
				aClrs[n].A = 0xFF;
			}
		}	break;

		case iePixelFormat::fBGRA: {
			iePCImage_fBGRA p = pim->fBGRA();
			iePCfBGRA p4 = p->PixelPtr();
			for (int n = 0; n < nSamples; n++) {
				iewBGRA w;
				ie_fBGRA2wBGRA(&w, &p4[aiOffs[n]], 1);
				ie_wBGRA2BGRA(&aClrs[n], &w, 1);
			}
		}	break;

		case iePixelFormat::fL: {
			iePCImage_fL p = pim->fL();
			iePCfL p1 = p->PixelPtr();
			for (int n = 0; n < nSamples; n++) {
				iewL w;
				ie_fL2wL(&w, &p1[aiOffs[n]], 1);
				ie_wL2L(&aClrs[n].B, &w, 1);
				aClrs[n].G = aClrs[n].B;
				aClrs[n].R = aClrs[n].B;
				aClrs[n].A = 0xFF;
			}
		}	break;

		}

		BYTE acB[nMaxSamples], acG[nMaxSamples], acR[nMaxSamples];
		int nSize = 0;
		for (int n = 0; n < nSamples; n++) {
			if (aClrs[n].A < 0x40) continue;
			acB[nSize] = aClrs[n].B;
			acG[nSize] = aClrs[n].G;
			acR[nSize] = aClrs[n].R;
			nSize++;
		}
		if (nSize >= 3) {
			BubbleSort<BYTE>(acB, nSize);
			BubbleSort<BYTE>(acG, nSize);
			BubbleSort<BYTE>(acR, nSize);
			nSize >>= 1;	// Select the Median value point for each color component
			return ieBGRA(acB[nSize], acG[nSize], acR[nSize], 0xFF);
		}
	}

	return ieBGRA(0x80, 0x80, 0x80, 0xFF);	// Default to medium gray if we didn't find enough non-transparent points
}


void ViewerWnd::OnPaint(HDC hDC, const RECT &rcDirtyRegion)
{
	if (!pimd || WindowType.bLayered) return;

	// Update pan and offset variables
	muiCoord xyOffs;
	xyOffs.x = (int(whClient.w) - int(pimd->DispX()) + 1)/2;
    if (xyOffs.x < 0) {
       	xyOffs.x = 0;
		if (xyPan.nX < 0) {
			xyPan.nX = 0;
		}
		else {
			int iMaxPanX = int(pimd->DispX()) - int(whClient.w);
			if (xyPan.nX > iMaxPanX) xyPan.nX = iMaxPanX;
		}
    } else {
       	xyPan.nX = 0;
    }

   	xyOffs.y = (int(whClient.h) - int(pimd->DispY()) + 1)/2;
    if (xyOffs.y < 0) {
       	xyOffs.y = 0;
		if (xyPan.nY < 0) {
			xyPan.nY = 0;
		}
		else {
			int iMaxPanY = int(pimd->DispY()) - int(whClient.h);
			if (xyPan.nY > iMaxPanY) {
				xyPan.nY = iMaxPanY;
			}
		}
    } else {
       	xyPan.nY = 0;
	}

	bool bDrawToDesktopDC = !pCfg->app.bWinVistaOrLater && !bPreventDrawToDesktop && IsMaximized();
	if (bDrawToDesktopDC) {
		// For fullscreen mode on Windows XP: Draw on desktop DC instead of window DC (this way we can overwrite the taskbar)
		if (bMultiMonitor) {
			hDC = NULL;		
			HMONITOR hMon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
			if (hMon) {
				MONITORINFOEX mix;
				mix.cbSize = sizeof(mix);
				if (GetMonitorInfo(hMon, &mix) && *mix.szDevice) {
					hDC = CreateDC(mix.szDevice, NULL, NULL, NULL);
				}
			} 
			if (!hDC) hDC = GetDC(HWND_DESKTOP);
		} else {
			hDC = GetDC(HWND_DESKTOP);
		}
	}

	// Draw borders/background?
	bool bAlphaBlendBg = pimd->HasAlpha() && (pimd->DispImage()->BGRA());
	ieBGRA clrBorder;
	if (bGlassBackground) {
		// Glass
		clrBorder = ieBGRA(0, 0, 0, 0);
	}
	else if ((pCfg->vwr.clrBackground.A & IE_USEFIXEDBGCOLOR) || !pimd || !pimd->OrigImage()) {
		// Default fixed colour
		clrBorder = pCfg->vwr.clrBackground;
		clrBorder.A = 0xFF;
	}
	else if (pimd->FrameParams().clrBackground.A != 0) {
		// Background from file meta data (from GIF, PNG, or auto-color)
		clrBorder = pimd->FrameParams().clrBackground;
	}
	else {
		// Auto color
		iePImage pim = pimd->OrigImage();
		for (;;) {
			iePImage pimPrev = pim->FrameParams().pimPrev;
			if (!pimPrev) {
				clrBorder = pim->FrameParams().clrBackground = ie_AutoBgColor(pim);
				break;
			}
			pim = pimPrev;
			if (pim->FrameParams().clrBackground.A != 0) {
				clrBorder = pim->FrameParams().clrBackground;
				break;
			}
		}
		pimd->FrameParams().clrBackground = clrBorder;
	}

	if (!hbrBorderBrush || (clrBorderBrush != clrBorder)) {
		if (hbrBorderBrush) DeleteObject(hbrBorderBrush);
		hbrBorderBrush = (clrBorder.A == 0xFF)	? CreateSolidBrush(RGB(clrBorder.R, clrBorder.G, clrBorder.B))
												: CreateAlphaBrush(clrBorder);
		clrBorderBrush = clrBorder;
	}

	RECT rc;

	if (bAlphaBlendBg || bPaintBackgroundOnly) {

		// Clear all screen with bg color
		RECT rc = { 0, 0, whClient.w, whClient.h };
		FillRect(hDC, &rc, hbrBorderBrush);

	} else {
		// Clear borders with bg color
		if ((xyOffs.x > 0) || (xyOffs.y > 0)) {

			if (xyOffs.x > 0) {
					
				rc.left = 0;
				rc.top = 0;
				rc.right = xyOffs.x;
				rc.bottom = whClient.h;
				if (ieClipRect(rc, rcDirtyRegion)) {
					FillRect(hDC, &rc, hbrBorderBrush);
				}

				rc.left = whClient.w-xyOffs.x;
				rc.right = whClient.w;
				if (ieClipRect(rc, rcDirtyRegion)) {
					FillRect(hDC, &rc, hbrBorderBrush);
				}

				if (xyOffs.y > 0) {

					rc.left = xyOffs.x;
					rc.top = 0;
					rc.right = whClient.w-xyOffs.x;
					rc.bottom = xyOffs.y;
					if (ieClipRect(rc, rcDirtyRegion)) {
						FillRect(hDC, &rc, hbrBorderBrush);
					}

					rc.top = whClient.h-xyOffs.y;
					rc.bottom = whClient.h;
					if (ieClipRect(rc, rcDirtyRegion)) {
						FillRect(hDC, &rc, hbrBorderBrush);
					}
				}

			} else {

				rc.left = 0;
				rc.top = 0;
				rc.right = whClient.w;
				rc.bottom = xyOffs.y;
				if (ieClipRect(rc, rcDirtyRegion)) {
					FillRect(hDC, &rc, hbrBorderBrush);
				}

				rc.top = whClient.h-xyOffs.y;
				rc.bottom = whClient.h;
				if (ieClipRect(rc, rcDirtyRegion)) {
					FillRect(hDC, &rc, hbrBorderBrush);
				}
			}
		}
	}

	if (!bPaintBackgroundOnly) {

		// Draw the invalidated part of the image!		
		rc.left = xyOffs.x;
		rc.top = xyOffs.y;
		rc.right = xyOffs.x + min(pimd->DispX(), whClient.w);
		rc.bottom = xyOffs.y + min(pimd->DispY(), whClient.h);

		if (ieClipRect(rc, rcDirtyRegion)) {

			pimd->DrawToDC(hDC, { rc.left, rc.top }, { xyPan.nX + (rc.left - xyOffs.x), xyPan.nY + (rc.top - xyOffs.y) }, { DWORD(rc.right - rc.left), DWORD(rc.bottom - rc.top) });
		}

		// Draw any text?
		for (int nOverlay = 0; nOverlay < 3; nOverlay++) {

			BYTE cOpts = pCfg->vwr.acOverlayOpts[nOverlay];

			ieTextInfoType eInfo = OverlayType2InfoType(cOpts>>4);
			if (eInfo >= ieTextInfoType::Count) continue;

			PCTCHAR pcszText;
			int nTextLen;
			if (pimd->Text()->Have(eInfo)) {
				pcszText = pimd->Text()->Get(eInfo);
				nTextLen = _tcslen(pcszText);
			} else {
				if (eInfo != ieTextInfoType::Comment) continue;
				nTextLen = 0;
				for (int n = 0; n < 9; n++) {
					ieTextInfoType eType = ieTextInfoType(int(ieTextInfoType::Keyword1) + n);
					if (pimd->Text()->Have(eType)) {
						PCTCHAR pcszKeyword = pimd->Text()->Get(eType);
						int nKeywordLen = _tcslen(pcszKeyword);
						if (nKeywordLen > nTextLen) {
							nTextLen = nKeywordLen;
							pcszText = pcszKeyword;
						}
					}
				}
				if (!nTextLen) continue;
			}

			const int iBorderDist = (3 * LOWORD(GetDialogBaseUnits())) / 4;

			rc.left = 0;
			rc.top = 0;
			DrawText(hDC, pcszText, nTextLen, &rc, DT_CALCRECT);

			if ((2*(rc.bottom-rc.top) <= (whClient.h - 2*iBorderDist)) && ((rc.right-rc.left) <= 2*(whClient.w - 2*iBorderDist))) {

				rc.left = 0;
				rc.top = 0;
				rc.right = whClient.w;
				rc.bottom = whClient.h;
				
				int fmt = DT_SINGLELINE | DT_NOPREFIX;
				
				switch (cOpts & 3) {
				case 0:
					rc.left += 2*iBorderDist;
					fmt |= DT_LEFT;
					break;
				case 1:
					rc.right -= 2*iBorderDist;
					fmt |= DT_RIGHT;
					break;
				case 2:
					fmt |= DT_CENTER;
					break;
				}

				switch ((cOpts>>2) & 3) {
				case 0:
					rc.top += iBorderDist;
					fmt |= DT_TOP;
					break;
				case 1:
					rc.bottom -= iBorderDist;
					fmt |= DT_BOTTOM;
					break;
				}
				
				int iOldBkMode = SetBkMode(hDC, TRANSPARENT);
				
				rc.right--;
				rc.bottom--;
				SetTextColor(hDC, RGB(255,255,255));
				DrawText(hDC, pcszText, nTextLen, &rc, fmt);
				
				rc.left++;
				rc.top++;
				rc.right++;
				rc.bottom++;
				SetTextColor(hDC, RGB(0,0,0));
				DrawText(hDC, pcszText, nTextLen, &rc, fmt);
				
				SetBkMode(hDC, iOldBkMode);
			}
		}
	}

	if (bDrawToDesktopDC) ReleaseDC(hwnd, hDC);

	ValidateRect(hwnd, &rcDirtyRegion);
}


void ViewerWnd::UpdatePainted(const muiCoord *pxyNewPos, const muiSize *pwhNewSize, bool bContentsChanged)
{
	if (pxyNewPos || pwhNewSize) {

		muiCoord xyPos = MUI_NOMOVE;
		muiSize whSize = MUI_NOSIZE;

		if (pxyNewPos) {
			xyPos = *pxyNewPos;
			xyPos -= { ciFrameXW, ciFrameYH + ciCaptionYH };
		}

		if (pwhNewSize) {
			whClient = whSize = *pwhNewSize;
			whSize += {2 * ciFrameXW, 2 * ciFrameYH + ciCaptionYH};
			if (whSize.w < ciMinWinXW) { 
				int nExtend = (ciMinWinXW - whSize.w);
				if (pxyNewPos) xyPos.x -= (nExtend+1)/2;
				whClient.w += nExtend; 
				whSize.w += nExtend;
			}

			Repaint(nullptr, true);	// NB: Painting it both before and after changing the size reduces flicker...
		}

		MoveWindow(xyPos, whSize, true);
		if (bContentsChanged || pwhNewSize) Repaint(nullptr, true);
	
	} else if (bContentsChanged) {
		Repaint(nullptr, true);
	}
}


void ViewerWnd::UpdateLayered(const muiCoord *pxyNewPos, const muiSize *pwhNewSize, bool bContentsChanged)
{
	if (pwhNewSize) {
		whClient = *pwhNewSize;
	}

	if (xyPan.nX < 0) xyPan.nX = 0;
    if (xyPan.nY < 0) xyPan.nY = 0;

	if (!pimd) {
		bContentsChanged = false;
	} else {

		if ((pimd->DispX() - xyPan.nX) < whClient.w) {
			xyPan.nX = max(0, int(pimd->DispX()) - int(whClient.w));
			if (pimd->DispX() < whClient.w) {
				whClient.w = pimd ->DispX();
			}
		}

		if ((pimd->DispY() - xyPan.nY) < whClient.h) {
			xyPan.nY = max(0, int(pimd->DispY()) - int(whClient.h));
			if (pimd->DispY() < whClient.h) {
				whClient.h = pimd->DispY();
			}
		}
	}

	if (!bContentsChanged && !pxyNewPos) return;

	ieXY xyPos;
	ieXY *pxyPos = nullptr;

	if (pxyNewPos) {
		xyPos.nX = pxyNewPos->x;
		xyPos.nY = pxyNewPos->y;
		pxyPos = &xyPos;
	}
	
	ieWH whSize;
	ieWH *pwhSize = nullptr;

	if (bContentsChanged && whClient.w && whClient.h) {
		whSize.nX = whClient.w;
		whSize.nY = whClient.h;
		pwhSize = &whSize;
	}

	pimd->DrawLayeredWindow(hwnd, pxyPos, pwhSize, xyPan);
}


void ViewerWnd::OnSetCursor(HWND hwndChild, WORD nHittest, WORD wMouseMsg)
{
	// Show wait?
	if (nShowWaitCursor > 0) {
		static HCURSOR hcurWait = NULL;
		if (!hcurWait) hcurWait = LoadCursor(NULL, IDC_WAIT);
		SetCursor(hcurWait);
		return;
	}

	// Set a cursor showing if the window is scrollable
	if ((nHittest != HTCLIENT) || !pimd) {
		muiAppWindow::OnSetCursor(hwndChild, nHittest, wMouseMsg);
		return;
	}

	bool bPanX = (pimd->DispX() > whClient.w);
	bool bPanY = (pimd->DispY() > whClient.h);
	bool bPanningCursor = (bPanX || bPanY) && !bAltDown && !bMidDown && !bFrozen;

	if (bPanningCursor) {
		if (bPanX && bPanY) {
			static HCURSOR hcurXY = NULL;
			if (!hcurXY) hcurXY = LoadCursor(g_hInst, MAKEINTRESOURCE(XYSCROLLCUR));
			SetCursor(hcurXY);
		} else if (bPanX) {
			static HCURSOR hcurX = NULL;
			if (!hcurX ) hcurX  = LoadCursor(g_hInst, MAKEINTRESOURCE(XSCROLLCUR)); 
			SetCursor(hcurX);
		} else if (bPanY) {
			static HCURSOR hcurY = NULL;
			if (!hcurY) hcurY  = LoadCursor(g_hInst, MAKEINTRESOURCE(YSCROLLCUR));
			SetCursor(hcurY);
		}
	} else {
		static HCURSOR hcur0 = NULL;
		if (!hcur0) hcur0 = LoadCursor(NULL, IDC_ARROW);
		SetCursor(hcur0);
	}
}


void ViewerWnd::OnTimer(WPARAM wTimerId)
{
	if (wTimerId == 1) {	// Animation timer

		if (pimd->IsAnimated() && pimd->SwitchFrame()) {
			
			UpdateWindow();

			int iDelay = pimd->FrameParams().msDisplayTime;
			if (iDelay >= 0) SetTimer(hwnd, 1, max(60, iDelay), NULL);

		} else {
			KillTimer(hwnd, 1);
		}
	
		return;
	}
	
	if (wTimerId == 42) {
		
		POINT ptCur;
		if (GetCursorPos(&ptCur) && (WindowFromPoint(ptCur) != hwnd)) {
			UpdateCaption(false);
			KillTimer(hwnd, 42);
			bAutoCaptionTimer = false;
			return;
		}

		SetTimer(hwnd, 42, 100, NULL);
		return;
	}
}


void ViewerWnd::UpdateWorkArea()
{
	bool bUseFullDisplay = IsMaximized() || WindowType.bIsTopMost;

	muiGetWorkArea(hwnd, xyWorkOrigo, whWorkArea, bUseFullDisplay);
}


void ViewerWnd::UpdateCaption(bool bCaptioned, bool bForceUpdate)
{
	if (iInhibitCaptionChange || bFrozen) return;

	bool bIsFullscreen = IsMaximized();
	if (bIsFullscreen || !pCfg->vwr.bHideCaption) bCaptioned = true;

	bool bLayered = !bCaptioned;

	if (!bForceUpdate && (bCaptioned == WindowType.bHasCaption) && (bLayered == WindowType.bLayered)) return;

	iInhibitCaptionChange++;
	iInhibitSizeChange++;

	bool bInitialized = pimd && (whClient.w > 0) && (whClient.h > 0);

	// Get screen-position of the upper-left corner of the client area
	muiCoord xyPos;
	if (bInitialized) {
		if (bIsFullscreen) {
			xyPos = xyWorkOrigo;
		} else {
			GetScreenPosOfWindow(xyPos);
			if (!WindowType.bLayered) xyPos += { ciFrameXW, ciFrameYH + ciCaptionYH };
			else if (pimd && pimd->HasOutline()) xyPos += { 1, 1 };
		}
	}

	// Turn on/off caption & layering

	if (bCaptioned) {

		//
		// Turn on caption mode (turn off layered mode)
		//
		WindowType.bHasCaption = true;

		if (pimd && WindowType.bLayered && !bIsFullscreen && pimd->HasOutline()) {	// Redraw it layered without outline - this is done to reduce flickering...
			pimd->DisableOutline();								
			UpdateLayered(&xyPos, &whClient, true);
		}
	
		muiSize whSize;
		if (bInitialized) {
			xyPos -= { ciFrameXW, ciFrameYH + ciCaptionYH };	// Adjust position and size for window frame + caption

			whSize = whClient;
			whSize += { 2 * ciFrameXW, 2 * ciFrameYH + ciCaptionYH };
			if (whSize.w < ciMinWinXW) {
				int wExtend = ciMinWinXW - whSize.w;
				xyPos.x -= (wExtend+1) / 2;
				whSize.w += wExtend;
				whClient.w += wExtend;
			}
		}

		DWORD dwStyle = WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
		if (IsVisible()) dwStyle |= WS_VISIBLE;
		SetWindowLong(hwnd, GWL_STYLE, dwStyle);					// Turn on caption

		UpdateGlassState(!bIsFullscreen);							// Turn on glass
		SetLayered(false);											// Turn off layering

		UINT uMoveFlags = SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOSENDCHANGING | SWP_NOOWNERZORDER | SWP_NOZORDER | SWP_DEFERERASE;
		if (!bInitialized) uMoveFlags |= (SWP_NOMOVE | SWP_NOSIZE);

		SetWindowPos(hwnd, nullptr, xyPos.x, xyPos.y, whSize.w, whSize.h, uMoveFlags);	// Update painted window
		
		UpdatePainted(nullptr, nullptr, true);							// Force repaint at once

	} else {

		//
		// Go to layered mode (turn off caption mode)
		//
		WindowType.bHasCaption = false;

		SetLayered(false);											// Turn off layering first - to ensure this is toggled on/off, otherwise the alpha blending state does not seem to be reset!
		SetLayered(true);											// Turn on layering

		if (bInitialized) {
			
			if (whClient.w > pimd->DispX()) {						// If image if offset to window, then move window since layered never has such an offset
				xyPos.x += (whClient.w - pimd->DispX() + 1) / 2;
			}
			if (whClient.h > pimd->DispY()) {
				xyPos.y += (whClient.h - pimd->DispY() + 1) / 2;
			}
		}

		if (pimd && !pimd->HasAlpha()) {
			pimd->SetOutline(ieBGRA(0,0,0));
		}

		DWORD dwStyle = WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;
		if (IsVisible()) dwStyle |= WS_VISIBLE;
		if (pimd && pimd->HasOutline()) dwStyle |= WS_BORDER;
		SetWindowLong(hwnd, GWL_STYLE, dwStyle);					// Turn off caption

		UpdateLayered(bInitialized ? &xyPos : NULL, bInitialized ? &whClient : NULL, bInitialized);	// Update layered window

		UpdateGlassState(false);									// Turn off glass
	}

	iInhibitSizeChange--;
	iInhibitCaptionChange--;
}


void ViewerWnd::UpdateGlassState(bool bEnable)
{
	// Turn Glass effect on/off?
	if (!pimd) return;
	bool bWantGlass = bEnable && pCfg->app.bWinVistaOrLater && ((pCfg->vwr.clrBackground.A & IE_USESOLIDBGCOLOR) == 0) && (pimd->HasAlpha() || ((whClient.w > pimd->DispX()) || (whClient.h > pimd->DispY())));

	bool bDispHasAlphaCh = pimd->DispImage() && (pimd->DispImage()->BGRA() || pimd->DispImage()->wBGRA());

	if (bWantGlass && !bDispHasAlphaCh) 
		bWantGlass = false;	// Can't do glass for CLUT/L/wL images that will be BitBlt'ed (since that results in an alpha channel that is all zero == transparent)

	if (bWantGlass != bGlassBackground) {

		if (!bWantGlass) {
			SetGlassEffect(false);			
			bGlassBackground = false;
		} else {
			bGlassBackground = SetGlassEffect(true);
		}
	}
}


void ViewerWnd::UpdateTitle()
{
	if (!pimd) return;

	TCHAR s[MAX_PATH + 64];
	PTCHAR p = s;
	*p = 0;

	if (bSlideShow) {
		_tcscpy(p, ieTranslate(_T("Slideshow")));
		p += _tcslen(p);
		_tcscpy(p, _T(" | "));
		p += 3;
	}

	if (pimd->Text()) {
		PCTCHAR pcszFile = pimd->Text()->Get(ieTextInfoType::SourceFile);
		if (pcszFile) {
			if (!pCfg->vwr.bFullPath) {
				PCTCHAR pcszName, pcszExt;
				ief_SplitPath(pcszFile, pcszName, pcszExt);
				pcszFile += (pcszName - pcszFile);
			}
		}
		else {
			pcszFile = pimd->Text()->Get(ieTextInfoType::Name);
		}
		if (pcszFile) {
			_tcscpy(p, pcszFile);
			p += _tcslen(p);
		}
	}

	PCTCHAR pcszPage = pimd->Text()->Get(ieTextInfoType::Page);
	if (pcszPage && *pcszPage) {
		_stprintf(p, _T(" [%s]"), pcszPage);
		p += _tcslen(p);
	}

    if (pCfg->vwr.bTitleRes) {
		PCTCHAR pszZ = pimd->Text()->Get(ieTextInfoType::SourceBitsPerPixel);
		if (!pszZ) {
			if (pimd->FrameParams().pimPrev) {
				iePCImage p2 = pimd->OrigImage();
				do {
					p2 = p2->FrameParams().pimPrev;
				} while (p2->FrameParams().pimPrev);
				pszZ = p2->Text()->Get(ieTextInfoType::SourceBitsPerPixel);
			}
			if (!pszZ) pszZ = _T("?");
		}
		_stprintf(p, _T(" | %d•%d•%s"), pimd->X(), pimd->Y(), pszZ);
		p += _tcslen(p);

		switch (pimd->OrigImage()->PixelFormat()) {
		case iePixelFormat::CLUT:
			_tcscpy(p, _T(" | i"));
			p += 4;
			break;
		case iePixelFormat::wL:
		case iePixelFormat::wBGRA:
			_tcscpy(p, _T(" | w"));
			p += 4;
			break;
		case iePixelFormat::fL:
		case iePixelFormat::fBGRA:
			_tcscpy(p, _T(" | HDR"));
			p += 6;
			break;
		}

		float fZoom = pimd->Adjustments()->GetZoom();
		if (fZoom != 1.0f) {
			int iZoom = int(1000.0f*fZoom + 0.5f);
			int iDec0 = (iZoom/100)%10;
			int iDec1 = (iZoom/10)%10;
			int iDec2 = iZoom%10;
			_tcscpy(p, _T(" | "));
			p += 3;
			_stprintf(p, iDec2 ? _T("%d.%c%c%cx") : (iDec1 ? _T("%d.%c%cx") : _T("%d.%cx")), iZoom/1000, TCHAR('0'+iDec0), TCHAR('0'+iDec1), TCHAR('0'+iDec2));
			p += _tcslen(p);
		}
	}

	_tcscpy(p, _T(" | "));
	p += 3;

	_tcscpy(p, _T(PROGRAMVERNAME));

	SetWindowText(hwnd, s);
}


void ViewerWnd::Size2Original()
{
	if (!pimd) return;

    bool bOldAutoSizeImage = pCfg->vwr.bAutoSizeImage;
	pCfg->vwr.bAutoSizeImage = false;

	if (pimd->Adjustments()->GetZoom() != 1.0) {
		pimd->Adjustments()->SetZoom(1.0);
		pimd->ZoomHasChanged();
	}
	
	xyPan = { 0, 0 };

    if (IsMaximized() || bFrozen) {
		UpdateWindow();
	} else {
		Size2Image();
	}

	UpdateTitle();

	pCfg->vwr.bAutoSizeImage = bOldAutoSizeImage;
}


void ViewerWnd::Size2Window(bool bThenSize2Image, bool bAllowLargerThanWorkArea, muiCoord *pxyNewPos, muiSize *pwhNewSize)
{
	if (!pimd) return;

	iInhibitSizeChange++;

	muiSize whWindow = pwhNewSize ? *pwhNewSize : whClient;

	double dFactor;
	if ((whWindow.w < 1) || (whWindow.h < 1)) {
		dFactor = 1.0/double(pimd->X());
	} else {
		bool bWtW = (double(pimd->X())/double(pimd->Y())) >= (double(whWindow.w)/double(whWindow.h));
		if ((bWtW && !bAllowLargerThanWorkArea) || (!bWtW && bAllowLargerThanWorkArea)) {
			dFactor = double(whWindow.w)/double(pimd->X());	// Image 'wider' than window
		} else {
			dFactor = double(whWindow.h)/double(pimd->Y());	// Image 'taller' than window
		}
	}

	if (pimd->Adjustments()->GetZoom() != dFactor) {
		
		pimd->Adjustments()->SetZoom(dFactor);
		pimd->ZoomHasChanged();
	}

	if (bThenSize2Image && !(bFrozen || IsMaximized())) {
		Size2Image();
	} else {
		UpdateWindow(pxyNewPos, pwhNewSize, true);
	}

	UpdateTitle();
	
	bInhibitAutoSize = false;

	iInhibitSizeChange--;
}


void ViewerWnd::Size2Image(bool bCenterOnLast) 
{
	if (!pimd) return;

	if (IsMaximized()) SendCommand(hwnd, CMD_GOWINDOWED);

	muiCoord xyPos;					// Window position
	muiSize whSize;					// Window size

	if (bRecenterWindow) {
		if (!bCenterOnLast) {
			// Center on display
			xyPos = xyWorkOrigo;
			xyPos += { whWorkArea.w >> 1, whWorkArea.h >> 1 };
		} else {
			// Center on same spot as prev image
			xyPos = { whClient.w >> 1, whClient.h >> 1 };
			ClientToScreen(xyPos);
		}
	} else {
		xyPos = { 0, 0 };
		ClientToScreen(xyPos);
	}

	whSize = { int(pimd->DispX()), int(pimd->DispY()) };
	
	// Ensure not larger than display
	bool bAutoSizeImage = false;

	muiSize muiMaxSize(whWorkArea);
	if (!pCfg->vwr.bHideCaption) {
		muiSize muiCaptionExtent{ 2 * ciFrameXW, 2 * ciFrameYH + ciCaptionYH };
		muiMaxSize -= muiCaptionExtent;
	}

	if (whSize.h > muiMaxSize.h) {
		if (pCfg->vwr.bAutoSizeImage && !bInhibitAutoSize) {
			whSize.w = int(double(whSize.w) * double(muiMaxSize.h) / double(whSize.h) + 0.5);
			bAutoSizeImage = true;
		}
		whSize.h = muiMaxSize.h;
	}

	if (whSize.w > muiMaxSize.w) {
		if (pCfg->vwr.bAutoSizeImage && !bInhibitAutoSize) {
			whSize.h = int(double(whSize.h) * double(muiMaxSize.w) / double(whSize.w) + 0.5);
			bAutoSizeImage = true;
		}
		whSize.w = muiMaxSize.w;
	}

	// Center on the desired spot
	if (bRecenterWindow) {
		xyPos -= { whSize.w / 2, whSize.h / 2 };
	}
	
	// Ensure that it's not completely off-screen
	if (xyPos.x >= xyWorkOrigo.x + muiMaxSize.w)
		xyPos.x = xyWorkOrigo.x + muiMaxSize.w - whSize.w;

	if (xyPos.x + whSize.w < xyWorkOrigo.x)
		xyPos.x = xyWorkOrigo.x;

	if (xyPos.y >= xyWorkOrigo.y + muiMaxSize.h)
		xyPos.y = xyWorkOrigo.y + muiMaxSize.h - whSize.h;

	if (xyPos.y + whSize.h < xyWorkOrigo.y)
		xyPos.y = xyWorkOrigo.y;

	if (!pCfg->vwr.bHideCaption && !bCenterOnLast) {
		xyPos.y += ciCaptionYH >> 1;
	}

	// Update window
	if (bAutoSizeImage) {
		Size2Window(false, false, &xyPos, &whSize);
	} else {
		iInhibitSizeChange++;

		UpdateWindow(&xyPos, &whSize, true);

		iInhibitSizeChange--;
	}
}


void ViewerWnd::OnNewImage() 
{
	bool bFirstImage = !(whClient.w | whClient.h);

	bool bIsFullscreen = IsMaximized();
	if (!bIsFullscreen && bFirstImage && bGoFullscreenOnFirst) {
		// Start in fullscreen option is on
		bIsFullscreen = true;
		WINDOWPLACEMENT wpl;	// Use SetWindowPlacement rather then ShowMaximized to start in full-screen, since this bypasses window animation
		wpl.length = sizeof(wpl);
		if (GetWindowPlacement(hwnd, &wpl)) {
			wpl.showCmd = SW_MAXIMIZE;
			SetWindowPlacement(hwnd, &wpl);
		}
	} 

	bInhibitAutoSize = false;

	if (fInitZoomFactor > 0.0f) {
		ZoomImage(fInitZoomFactor);
		fInitZoomFactor = -1.0f;
	}

	bDontSize2WindowOnRestore = bIsFullscreen;

	if (bIsFullscreen) SendCommand(hwnd, CMD_GOFULLSCREEN);
	else if (bFirstImage) UpdateCaption(false, true);

	bool bWasAlpha = bAlphaImage;
	bool bWasGlass = bGlassBackground;

	UpdateGlassState(!bIsFullscreen && WindowType.bHasCaption);
	bAlphaImage = pimd->DispImage()->AlphaType() != ieAlphaType::None;

	if (WindowType.bLayered && !bFirstImage && ((bWasGlass != bGlassBackground) || (bWasAlpha != bAlphaImage))) {
		SetLayered(false);	// If layered window and glass or alpha state has changed, then we need to toggle layering off and on again to update Windows transparancy (alphablt) state 
		SetLayered(true);
	}
	
	if (bFrozen) {
		if (bFirstImage) {
			Size2Image(false);
		} else if ((pimd->DispX() > whClient.w) || (pimd->DispY() > whClient.h)) {
			Size2Window(false);
		} else {
			UpdateWindow();
		}
	} else if (bIsFullscreen) {
		if (	(pCfg->vwr.bAutoSizeImage && !pCfg->vwr.bAutoSizeOnlyShrink)
			||	(	(pCfg->vwr.bAutoSizeImage || pCfg->vwr.bAutoShrinkInFullscreen) && ((whWorkArea.w < pimd->X()) || (whWorkArea.h < pimd->Y())))
		) {
			Size2Window(false);
		} else {
			UpdateWindow();
		}
	} else {
		Size2Image(!bFirstImage);
    }

	UpdateTitle();
	
	if (bFirstImage) SetVisible(true);		//NB, turning this on *after* UpdateCaption() above, will disable the Windows "fade in" animation, good or bad?
	if (!bFrozen) SendCommand(hwnd, CMD_BRINGTOFOREGROUND);

	if (pimd->IsAnimated()) {
		int iDelay = pimd->FrameParams().msDisplayTime;
		if (iDelay >= 0) SetTimer(hwnd, 1, max(50, iDelay), NULL);
	}
}


bool ViewerWnd::LoadIt(PCTCHAR pcszFile, bool bCacheNextImage, bool bShowErrors)
{
	TCHAR sz[MAX_PATH + 64];
	PCTCHAR pcszName, pcszExt;
	ief_SplitPath(pcszFile, pcszName, pcszExt);

	// Special case: Image Eye files (.IES, .IEI, .IEA)
	if ((pcszExt[0] == '.') && ((pcszExt[1] == 'i') || (pcszExt[1] == 'I')) && ((pcszExt[2] == 'e') || (pcszExt[2] == 'E'))) {
		wchar_t cType = pcszExt[3];
		if (cType > 'Z') cType += 'A' - 'a';

		if (cType == 'S') {
			// Slide show - launch it!
			RunSlideShow(*this, pcszFile);
			return true;
		}
		else if (cType == 'I') {
			// Index - spawn instance for directory containing it!
			SpawnIndex(pcszFile);
			return false;
		}
		else if (cType == 'A') {
			// Adjustment file - load associated image file!
			int n = (pcszExt - pcszFile);
			memcpy(sz, pcszFile, n*sizeof(TCHAR));
			sz[n] = 0;
			return LoadIt(sz, bCacheNextImage, bShowErrors);
		}
		else {
			return false;
		}
	}

	// Ok, prepare for loading an image file
	SetPriorityClass(GetCurrentProcess(), ABOVE_NORMAL_PRIORITY_CLASS);

	bool bIsCached = g_ieFM.ImageCache.IsCached(pcszFile);
	if (!bIsCached) SendMessage(hwnd, WM_COMMAND, CMD_ONSHOWHIDEWAITCURSOR | (1 << 16), 0);

	SetWindowText(hwnd, pcszName);

	// And load it!
	iePImage pim = nullptr;
	ieResult r = g_ieFM.ReadImage(pim, pcszFile, true, g_pProgress);

	if (ieIsOk(r)) {

		g_pProgress->Progress(100);

		if (pimd) {
			if (pimd->IsAnimated()) {
				KillTimer(hwnd, 1);
			}
			pimd->Release();
		}

		pimd = ieImageDisplay::Create(pim, true);
		if (pimd) pimd->PrepareDisp();
	}

	SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);

	if (ieFailed(r) && bShowErrors) {
		PCTCHAR pcszErrorMsg;
		if (r == IE_E_FILEOPEN) {
			_stprintf(sz, _T("Couldn't open file: \"%s\""), pcszFile);
			pcszErrorMsg = sz;
		}
		else {
			pcszErrorMsg = ie_GetErrorDescription(r);
		}
		MessageBox(hwnd, pcszErrorMsg, _T(PROGRAMNAME), MB_OK);
	}

	// Save last directory
	int i = pcszName - pcszFile;
	if (i > 1) {
		memcpy(sz, pcszFile, --i*sizeof(TCHAR));
		sz[i] = 0;
		pCfgInst->RecentPaths.Set(sz);
	}
	else {
		*sz = 0;
	}

	// Fix up the window et c!
	g_pProgress->Hide();

	if (ieIsOk(r)) SendMessage(hwnd, WM_COMMAND, CMD_ONNEWIMAGE, 0);

	if (!bIsCached) SendMessage(hwnd, WM_COMMAND, CMD_ONSHOWHIDEWAITCURSOR, 0);

	if (ieFailed(r)) return false;

	// Save last file name
	pCfgInst->SetLastFile(pcszFile);

#ifdef IE_SUPPORT_IMAGECACHE
	// Cache the image for future use
	if (pimd) g_ieFM.ImageCache.CacheImage(pimd->OrigImage());
#endif

#ifdef IE_SUPPORT_DIRCACHE
	// Cache file names from the directory
	if (*sz) {
		ieIDirectoryEnumerator *pOldEnum = pEnum;
		pEnum = g_ieFM.DirCache.CreateEnumerator(sz);
		if (pOldEnum) pOldEnum->Release();
	}
#endif

	// Tell image cache to decode next image
#ifdef IE_SUPPORT_IMAGECACHE
	if (bCacheNextImage) {
		PCTCHAR pcszThisFile = pimd->Text()->Get(ieTextInfoType::SourceFile);
		if (*pcszThisFile && (*pcszThisFile != '<')) {
			g_ieFM.ImageCache.CacheImageAsync(pcszThisFile, ieSearchDirection::DefaultFile, true, nullptr, g_pProgress);
		}
	}
#endif

	return true;
}


bool ViewerWnd::LoadNewImage(PCTCHAR pcszFile, bool bDisplayProgress, bool bCacheNextImage, bool bShowErrors)
{
	if (bDisplayProgress) {
		if (!g_pProgress) g_pProgress = g_CreateProgress(hwnd);
		g_pProgress->Progress(0);
		g_pProgress->Show();
	}

	if (pcszFile) {

		// Explicitly given file name
		return LoadIt(pcszFile, bCacheNextImage, bShowErrors);
	}

	// Ask user for a file name
	TCHAR szFile[4 * MAX_PATH], sz[MAX_PATH + 10];

	if (!ShowOpenFileDialog(hwnd, szFile, sizeof(szFile) / sizeof(TCHAR), true)) {
#ifdef IE_SUPPORT_IMAGECACHE
		g_ieFM.ImageCache.CancelAsync();
#endif
		return false;
	}

	PCTCHAR pcszNextFile = szFile + _tcslen(szFile) + 1;

	if (!*pcszNextFile) {

		// Single file selected
		return LoadIt(szFile, bCacheNextImage, bShowErrors);

	}
	else {

		// Multiple files selected (first entry in a szz-list holds the path)
		bool bSpawn = false;

		do {
			_stprintf(sz, _T("%s\\%s"), szFile, pcszNextFile);
			if (bSpawn) {
				if (!SpawnViewer(sz)) return false;
			}
			else {
				if (!LoadIt(sz, bCacheNextImage, bShowErrors)) return false;
			}

			pcszNextFile += _tcslen(pcszNextFile) + 1;
			bSpawn = true;

		} while (*pcszNextFile);

		return true;
	}
}


bool ViewerWnd::LoadNextPrevImage(ieSearchDirection eDirection, bool bSpawnNewViewer, bool bCacheNext)
{
#ifndef IE_SUPPORT_DIRCACHE
	return false;
#else
	if (!pEnum) return false;

	bool bHaveFileList = pEnum->IsCachingDone();
	if (!bHaveFileList) SendMessage(hwnd, WM_COMMAND, CMD_ONSHOWHIDEWAITCURSOR | (1 << 16), 0);

	if (pimd->IsMultiPage() && pimd->SwitchFrame(eDirection)) {

		pimd->Adjustments()->SetZoom(pimd->Adjustments()->GetZoom());
		pimd->ZoomHasChanged();
		if (!bHaveFileList) SendMessage(hwnd, WM_COMMAND, CMD_ONSHOWHIDEWAITCURSOR, 0);
		SendMessage(hwnd, WM_COMMAND, CMD_ONNEWIMAGE | (CMD_SIZE2WINDOW << 16), 0);// Fix up the window!
		return true;
	}

	if (!pimd->Text() || !pimd->Text()->Have(ieTextInfoType::SourceFile)) return false;

	TCHAR szFile[MAX_PATH];
	_tcscpy(szFile, pimd->Text()->Get(ieTextInfoType::SourceFile));

	bool r;

	do {
		r = g_ieFM.FindFile(pEnum, eDirection, szFile, szFile);

		if (!r) {
			MessageBeep(MB_OK);
			if (eDirection == ieSearchDirection::NextFile) r = g_ieFM.FindFile(pEnum, ieSearchDirection::FirstFile, szFile, szFile);
			else if (eDirection == ieSearchDirection::PrevFile) r = g_ieFM.FindFile(pEnum, ieSearchDirection::LastFile, szFile, szFile);
			if (!r) break;
			g_ieFM.SetFindFileDefaultDirection(eDirection);
		}

		if (bSpawnNewViewer) 
			r = SpawnViewer(szFile);
		else 
			r = LoadNewImage(szFile, true, bCacheNext, false);

	} while (!r);

	if (!bHaveFileList) SendMessage(hwnd, WM_COMMAND, CMD_ONSHOWHIDEWAITCURSOR, 0);

	return r;
#endif //IE_SUPPORT_DIRCACHE
}


void ViewerWnd::Trim2Window()
{
	if (!pimd) return;

	if ((pimd->DispX() <= whClient.w) && (pimd->DispY() <= whClient.h)) return;
	int iXo, iYo, iXw, iYw;
	if (pimd->DispX() > whClient.w) {
		iXo = xyPan.nX;
		iXw = whClient.w;
	} else {
		iXo = 0;
		iXw = pimd->DispX();
	}
	if (pimd->DispY() > whClient.h) {
		iYo = xyPan.nY;
		iYw = whClient.h;
	} else {
		iYo = 0;
		iYw = pimd->DispY();
	}
	xyPan.nX = xyPan.nY = 0;

	double dS = (double)pimd->X() / (double)pimd->DispX();
	float fZoom = pimd->Adjustments()->GetZoom();

	ieXY xy;
	ieWH wh;
	pimd->Adjustments()->GetTrim(xy, wh);
	xy.nX += SDWORD(iXo / fZoom);
	xy.nY += SDWORD(iYo / fZoom);
	wh.nX = DWORD(iXw / fZoom);
	wh.nY = DWORD(iYw / fZoom);
	pimd->Adjustments()->SetTrim(xy, wh);
	pimd->AdjustmentsHasChanged();

	UpdateTitle();

	UpdateWindow();
}


void ViewerWnd::ZoomImage(double dNewZoom)
{
	if (!pimd) return;
	if (dNewZoom == pimd->Adjustments()->GetZoom()) return;

	// Validate new size
	int iNewX = int(pimd->X()*dNewZoom + 0.5);
	int iNewY = int(pimd->Y()*dNewZoom + 0.5);
	if ((iNewX < 2) || (iNewY < 2)) return;
	
	QWORD qwImageBytes = QWORD(iNewX) * iNewY * ((pimd->DispImage() && (pimd->DispImage()->CLUT())) ? 1 : 4);

	bInhibitAutoSize = (iNewX > whWorkArea.w) || (iNewY > whWorkArea.h);

	// Zoom it
	bool bShowWait = (qwImageBytes >= 128 * 1024 * 1024);
	if (bShowWait) SendMessage(hwnd, WM_COMMAND, CMD_ONSHOWHIDEWAITCURSOR | (1 << 16), 0);

	double dOldZoom = pimd->Adjustments()->GetZoom();
	muiCoord xyPanCenter;
	xyPanCenter.x = int(((xyPan.nX + (whClient.w/2)) / dOldZoom) + 0.5);
	xyPanCenter.y = int(((xyPan.nY + (whClient.h/2)) / dOldZoom) + 0.5);

	pimd->Adjustments()->SetZoom(dNewZoom);
	pimd->ZoomHasChanged();

	if (bShowWait) SendMessage(hwnd, WM_COMMAND, CMD_ONSHOWHIDEWAITCURSOR, 0);

	if (pimd->Adjustments()->GetZoom() < dNewZoom) {
		// Failed due to too big image, try to revert to old state
		HCURSOR hcurOld = SetCursor(LoadCursor(NULL, IDC_NO));

		pimd->Adjustments()->SetZoom(dOldZoom);
		pimd->ZoomHasChanged();

		SetCursor(hcurOld);
		if (pimd->Adjustments()->GetZoom() == dOldZoom) return;
	}

	// Find new position and size of window to fit new zoom state?
	bool bMoveAndResizeWindow = !IsMaximized() && !bFrozen;
	muiCoord xyPos;
	muiSize whSize;

	if (bMoveAndResizeWindow) {
		xyPos = { whClient.w / 2, whClient.h / 2 };			// Center of old window in screen coords
		ClientToScreen(xyPos);

		whSize.w = min(pimd->DispX(), whWorkArea.w);		// Ensure not larger than display
		whSize.h = min(pimd->DispY(), whWorkArea.h);

		xyPos -= { whSize.w / 2, whSize.h / 2 };			// Now upper left corner of new window pos
	} else {
		whSize = whClient;
	}

	// Update window
	xyPan.nX = int(xyPanCenter.x*dNewZoom + 0.5) - (whSize.w/2);
	xyPan.nY = int(xyPanCenter.y*dNewZoom + 0.5) - (whSize.h/2);

	iInhibitSizeChange++;

	UpdateWindow(bMoveAndResizeWindow ? &xyPos : nullptr, bMoveAndResizeWindow ? &whSize : nullptr, true);
	
	UpdateTitle();
	
	iInhibitSizeChange--;

	// Update cursor?
	POINT pt;
	GetCursorPos(&pt);
	if (WindowFromPoint(pt) == hwnd) {
		OnSetCursor(hwnd, HTCLIENT, WM_MOUSEMOVE);
	}
}


//-----------------------------------------------------------------------------

bool SpawnViewer(PCTCHAR pcszFile, HWND hwndOpenDlgParent)
{
	TCHAR szFile[4 * MAX_PATH];
	if (!pcszFile) {

		if (!ShowOpenFileDialog(hwndOpenDlgParent, szFile, sizeof(szFile) / sizeof(TCHAR), true)) {
#ifdef IE_SUPPORT_IMAGECACHE
			g_ieFM.ImageCache.CancelAsync();
#endif
			return false;
		}

		pcszFile = szFile;
	}

	if (pCfg->app.bSpawnNewProcess) {

		TCHAR szCmd[2 + IE_FILENAMELEN];
		if (pcszFile) {
			_stprintf(szCmd, _T("\"%s\""), pcszFile);
			pcszFile = szCmd;
		}

		if (!mdSpawnExe(pCfgInst->szExePath, pcszFile, false, false)) {
			return false;
		}
	}
	else {

		Viewer(pcszFile);
	}

	return true;
}


//-----------------------------------------------------------------------------

void Viewer(PCTCHAR pcszFileName, PCTCHAR pcszzOptions)
{
	PCTCHAR pcszTitle = nullptr;
	int iXPos = CW_USEDEFAULT, iYPos = CW_USEDEFAULT;
	bool bIndex = false, bFullScreen = false, bFrozen = false, bTopMost = false;
	float fZoomTo = -1;

	if (pcszzOptions) for (PCTCHAR p = pcszzOptions; *p; p += _tcslen(p) + 1) {

		if ((*p != '-') && (*p != '/')) continue; 		// Switch?
		p++;

		if (!_tcsicmp(p, _T("INDEX"))) {
			bIndex = true;
		}
		else if (!_tcsicmp(p, _T("FREEZE"))) {
			bFrozen = true;
		}
		else if (!_tcsicmp(p, _T("FULLSCREEN"))) {
			bFullScreen = true;
		}
		else if (!_tcsnicmp(p, _T("POS="), 4)) {
			p += 4;
			iXPos = _tstoi(p);
			while ((*p != ',') && (*p != ' ') && (*p != 0)) p++;
			iYPos = _tstoi(++p);
		}
		else if (!_tcsnicmp(p, _T("ZOOM="), 5)) {
			p += 5;
			fZoomTo = _tstof(p);
		}
		else if (!_tcsnicmp(p, _T("TITLE="), 6)) {
			pcszTitle = p + 6;
		}
		else if (!_tcsicmp(p, _T("TOPMOST"))) {
			bTopMost = true;
		}
	}

	if (bIndex) {
		Index(pcszFileName, pcszzOptions);
		return;
	}

	g_ieFM.EnableMultiFrame(true);

	g_bLoadNewDontSpawnIndexHack = true;
	g_bLoadNewSpawnIndexLaterHack = false;

	ViewerWnd *pWnd = new ViewerWnd(pcszFileName, pcszTitle, { iXPos, iYPos }, fZoomTo, bFullScreen, bFrozen, bTopMost);
	if (!pWnd) return;

	g_bLoadNewDontSpawnIndexHack = false;

	if (!pWnd->HaveImage()) {
		PostMessage(pWnd->WindowHandle(), WM_CLOSE, 0, 0);
		if (g_bLoadNewSpawnIndexLaterHack) {
			g_bLoadNewSpawnIndexLaterHack = false;
			Index();
		}
		return;
	}

	pWnd->SetAccelerators(VIEWER_MENU);

	pWnd->Run();
}
