//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Image Eye main include file (for shared stuff)
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


//-----------------------------------------------------------------------------

#define PROGRAMNAME			"Image Eye"			// NOTE! You *must* change this for your own versions!
#define PROGRAMVERSION		"9.1"
#define PROGRAMVERNAME		PROGRAMNAME " v" PROGRAMVERSION
#define PROGCOMPANY			"FMJ-Software"		// NOTE! You *must* change this for your own versions!


//-----------------------------------------------------------------------------

#include "ieC++.h"
#include "Config.h"
#include "Translator.h"


//-----------------------------------------------------------------------------
// In Main.cpp
//-----------------------------------------------------------------------------

#define ieTranslate(t)							g_Language.Translate(t)
#define ieTranslateCtrls(h)						g_Language.TranslateCtrls(h)

extern HINSTANCE g_hInst;						// Global instance handle for loading resources

extern iePProgress g_pProgress;					// Application global file-load progress (sometimes nullptr, sometimes non-nullptr but hidden - when pre-caching or previewing files - it still needs to be available in case we want to show the file being processed)

extern ieFileManager g_ieFM;					// Global instance of image file manager

extern bool g_bLoadNewDontSpawnIndexHack;
extern bool g_bLoadNewSpawnIndexLaterHack;

extern void OpenHelp(HWND hwnd);				// Launches HTML-help

extern iePProgress g_CreateProgress(HWND hwndParent);

extern bool ShowOpenFileDialog(HWND hwnd, PTCHAR pszFileName, int nMaxFileNameLen, bool bMultiple);

extern bool ie_CopyFile(HWND hwnd, PCTCHAR pcszFile, bool bMoveFile = false, int nNumFiles = 1);
extern bool ie_CopyFile(HWND hwnd, PCTCHAR pcszDstFile, PCTCHAR pcszSrcFile);

extern bool ie_RenameFile(HWND hwnd, PTCHAR pszFile);

extern bool ie_DeleteFileQuery(HWND hwnd, PCTCHAR pcszFile, int nNumFiles = 1);

extern bool ie_DeleteFile(HWND hwnd, PCTCHAR pcszFile, bool bEraseFile = false, int nNumFiles = 1);

extern void NextPrevWindow(HWND hwndThis, bool bPrev);

extern bool SaveImage(HWND hwnd, iePImageDisplay pimd, PCTCHAR pcszFile, bool bSaveOrig);

extern bool CaptureWindow(iePImageDisplay &pimd, bool bFullScreen);

extern void SetDesktopWallpaper(HWND hwnd, iePImageDisplay pimd);

extern void RemoveAutoLoad(PCTCHAR pcszFile);

extern void AddAutoLoad(HWND hwnd, PCTCHAR pcszFile, float fZoom);

extern void CloseAllImageEyeWindows();

extern bool ReceiveFileInFirstProcess(DWORD dwData, const void *pData, DWORD cbData);

extern bool HandOverFileToFirstProcess(PCTCHAR pcszFile, PCTCHAR pcszzOptions);


//-----------------------------------------------------------------------------
// In Viewer.cpp
//-----------------------------------------------------------------------------

struct ieIViewer {								// Viewer callback interface
	virtual bool LoadNewImage(PCTCHAR pcszFile, bool bDisplayProgress = true, bool bCacheNextImage = false, bool bShowErrors = true) = 0;
	virtual HWND GetHWnd() const = 0;
};

extern bool SpawnViewer(PCTCHAR pcszFile = nullptr, HWND hwndOpenDlgParent = HWND_DESKTOP);

extern void Viewer(PCTCHAR pcszFileName = nullptr, PCTCHAR pcszzOptions = nullptr);


//-----------------------------------------------------------------------------
// In Index.cpp
//-----------------------------------------------------------------------------

extern bool SpawnIndex(PCTCHAR pcszFolder);

extern void Index(PCTCHAR pcszFileName = nullptr, PCTCHAR pcszzOptions = nullptr);


//-----------------------------------------------------------------------------
// In ImageAdjustment.cpp
//-----------------------------------------------------------------------------

extern void ShowImageAdjustmentDialog(HWND hwnd, iePImageDisplay &pimd);


//-----------------------------------------------------------------------------
// In SlideShow.cpp
//-----------------------------------------------------------------------------

extern bool CreateSlideScript(PTCHAR pszScriptFile, HWND hwndDlgParent = HWND_DESKTOP, PCTCHAR pcszDefaultDir = nullptr, bool bAutoOptions = false);	// in SlideShow.cpp

extern bool RunSlideShow(ieIViewer &Viewer, PCTCHAR pcszScriptFile);


//-----------------------------------------------------------------------------
// In Clipboard.cpp
//-----------------------------------------------------------------------------

extern bool Clipboard_Query();
extern bool Clipboard_Read(ieIViewer &Viewer);
extern bool Clipboard_Write(HWND hwnd, iePImage pim);
