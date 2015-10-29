//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Configuration/user settings
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

#include <shobjidl.h>
#include "ieTextConv.h"
#include "mdSystem.h"
#include "mdDlgCtrls.h"
#include "mdRegStore.h"
#include "../Res/resource.h"


extern void GIFLZW_Decompress(iePILUT pDst, DWORD cbMaxDst, PCBYTE pbSrc, DWORD cbSrc);							// Borrow from ief_gif.cpp
extern DWORD GIFLZW_Compress(PBYTE pbDst, iePCILUT pSrc, DWORD nX, DWORD nY, DWORD nBPL, int iBitsPerByte);		// Borrow from ief_gif.cpp


//------------------------------------------------------------------------------------------

Config	*		pCfg = nullptr;
ConfigInstance *pCfgInst = nullptr;


//------------------------------------------------------------------------------------------

extern "C" unsigned long crc32(unsigned long crc, const unsigned char *buf, unsigned len);	// Borrow CRC32-calculation from Z-lib

void ShowSetupOptionsDialog();	// Forward declare


//------------------------------------------------------------------------------------------

static const char *pcszRegOptionsName = "Options8";
static const char *pcszRegRecentName = "RecentList";
static const char *pcszRegDictionaryName = "Dictionary8";


void Config::ReadFromRegistry()
{
	// Get and ev. set local machine info:
	char szRegPath[MAX_PATH];
	sprintf(szRegPath, "Software\\" PROGCOMPANY "\\%s", PROGRAMNAME);

	mdRegStore rs(HKEY_CURRENT_USER, szRegPath, true);

	// Check for previously installed program version
	TCHAR s[64];
	static const char *pcszAppVer = "i-Eye-Ver";
	rs.GetValue(pcszAppVer, s, sizeof(s)/sizeof(TCHAR), _T(""));

	bool bFirstTimeRun = false, bLeaveNoTrace = false;
	bool bNewVer = (_tcscmp(s, _T(PROGRAMVERSION)) < 0);
	if (bNewVer) {
		if (*s <= '0') {
			bFirstTimeRun = true;
			mdRegStore rsAppPath(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App Paths", true);
			if (!rsAppPath.ExistsValue(_T(PROGRAMNAME ".exe")))
				bLeaveNoTrace = true;
		}
		if (!bLeaveNoTrace) {
			mdRegStore rsWrite(HKEY_CURRENT_USER, szRegPath);
			rsWrite.SetValue(pcszAppVer, _T(PROGRAMVERSION));
		}
	}

	if (!bFirstTimeRun) {

		// Read options from the registry as one big data chunk
		PBYTE pcOptions = PBYTE(this);
		DWORD cbOptions = DWORD(LONG_PTR(&acRecentPaths) - LONG_PTR(pcOptions));
		rs.GetValue(pcszRegOptionsName, pcOptions, cbOptions, 0);

		// Read recent paths list
		rs.GetValue(pcszRegRecentName, (void *)&acRecentPaths, sizeof(acRecentPaths), 0);
	}

	if (bFirstTimeRun) {
		
		// Set default values
		iem_Zero(this, sizeof(*this));

		strcpy(app.acLanguage, "English - US");
		app.bSaveRecent = true;
		app.cFolderViewStyle = 'd';

		vwr.bHideCaption = true;
		vwr.bTitleRes = true;
		vwr.bAutoSizeImage = true;
		vwr.bAutoSizeOnlyShrink = true;
		vwr.bAutoShrinkInFullscreen = true;
		vwr.acOverlayOpts[0] = (3<<4)|(1<<2)|(2);
		vwr.acOverlayOpts[1] = (0<<4)|(1<<2)|(1);
		vwr.acOverlayOpts[2] = (0<<4)|(1<<2)|(0);
		vwr.clrBackground = ieBGRA(128, 128, 128, 0);
	
		sli.bRunMaximized = true;
		sli.eDelayType = 1;	// 1=Key-press
		sli.dwDelayTime10 = 80;
		sli.bSelfDelete = true;
		sli.bSortFiles = true;
	
		idx.bShowDirs = true;
		idx.cShowNameLines = 1;
		idx.bShowComment = true;
		idx.bShowRes = true;
		idx.iIconX = 160;
		idx.iIconY = 160;
		idx.iIconSpacing = 10;
		idx.cIconArrange = eArrangeByName;
		idx.bCompressCache = true;
		idx.bUseSizeFromCache = true;
		idx.bNumFilesTitle = true;
		idx.clrBackground = ieBGRA(255, 183, 94, 0);
	}

	if (app.dwDictionarySize) {

		// Read saved translation dictionary from the registry as one big data chunk
		pCfgInst->OpenSharedDictionary();

		bool bOK = pCfgInst->pDict != nullptr;
		if (bOK) {

			DWORD cbMaxComp = 2*app.dwDictionarySize;
			PBYTE pcComp = new BYTE[cbMaxComp];
			bOK = pcComp != nullptr;
			if (bOK) {
			
				DWORD cbComp = rs.GetValue(pcszRegDictionaryName, pcComp, cbMaxComp, 0);
				bOK = (cbComp != 0);

				if (bOK) GIFLZW_Decompress((iePILUT)pCfgInst->pDict, pCfg->app.dwDictionarySize, pcComp, cbComp);

				delete[] pcComp;
			}
		}

		if (bOK) {
			g_Language.SelectDictionary(pCfgInst->pDict);
		} else {
			pCfgInst->CloseSharedDictionary();
			pCfg->app.dwDictionarySize = 0;
		}
	}

	// This data is always updated (by the first instance), regardless of what's stored in the registry:
	app.iInstances = 1;
	app.bLeaveNoTrace = bLeaveNoTrace;
	app.dwReportPos = 0;

	app.bWinVistaOrLater = true;
#ifndef __X64__
	OSVERSIONINFO osvi;
	osvi.dwOSVersionInfoSize = sizeof(osvi);
	if (GetVersionEx(&osvi) && (osvi.dwMajorVersion < 6)) {
		// Windows says we are pre-Vista, but we may be in compatiblity mode (e.g. when launched from InnoSetup), then it reports Windows XP or earlier, thus now check if GetProductInfo() is available - it was introduced in Vista/Server 2008.
		if (!GetProcAddress(GetModuleHandle(_T("kernel32.dll")), "GetProductInfo")) {
			// Ok, we're really in XP
			app.bWinVistaOrLater = false;
			pCfg->vwr.clrBackground.A &= ~IE_USESOLIDBGCOLOR;	// Turn off glass effect
			pCfg->idx.clrBackground.A &= ~IE_USESOLIDBGCOLOR;
		}
	}
#endif

	if (bFirstTimeRun && !app.bLeaveNoTrace) {

		ShowSetupOptionsDialog();		// Ask for options	
		OpenHelp(NULL);					// Show manual
	}
}


void Config::WriteToRegistry()
{
	if (app.bLeaveNoTrace) return;
	if (!app.bSaveRecent) pCfgInst->RecentPaths.Clear();
	
	// Check CRC32 to find out if anything has changed that needs to be stored! (NB: CRC32 could fail to detect a change, but it's unlikely... => intentional bug in the interest of speed! :-D )
	PCBYTE pcOptions = PCBYTE(this);
	DWORD cbOptions = DWORD(LONG_PTR(&acRecentPaths) - LONG_PTR(pcOptions));

	DWORD dwNewCRC32 = crc32(0, pcOptions + sizeof(dwCRC32), cbOptions - sizeof(dwCRC32));	
	bool bOptionsChanged = (dwCRC32 != dwNewCRC32);

	// Have recent files list changed?
	bool bRecentChanged = pCfgInst->RecentPaths.IsModified();

	if (!bOptionsChanged && !bRecentChanged) return;

	// Store changes:
	
	char szRegPath[MAX_PATH];
	sprintf(szRegPath, "Software\\" PROGCOMPANY "\\%s", PROGRAMNAME);

	mdRegStore rs(HKEY_CURRENT_USER, szRegPath);

	if (bOptionsChanged) {
		// Store options

		dwCRC32 = dwNewCRC32;

		rs.SetValue(pcszRegOptionsName, REG_BINARY, pcOptions, cbOptions);

		// (Language translation is not saved here - it's saved only after it is changed in the properties)
	}

	if (bRecentChanged) {
		// Store recent paths list

		PCBYTE pcRecent = PCBYTE(&acRecentPaths);
		DWORD cbRecent = pCfgInst->RecentPaths.UsedDataSize();

		if (cbRecent) {
			rs.SetValue(pcszRegRecentName, REG_BINARY, pcRecent, cbRecent);
		} else {
			rs.DeleteValue(pcszRegRecentName);
		}
	}
}


//------------------------------------------------------------------------------------------

static INT_PTR CALLBACK SetupOptionsDlg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
// Setup options dialog message handler
{
	TCHAR sz[MAX_PATH], szCurLanguage[128];
	int n;
	HANDLE hff;
	WIN32_FIND_DATA ffd;

	switch (message) {

	case WM_INITDIALOG:		

		// Init controls
		SendMessage(hwnd, WM_COMMAND, 9999, 0);
		pCfgInst->LoadLanguage();
		ieTranslateCtrls(hwnd);

		CtlButn::SetCheck(hwnd, SUO_C_ASSOCIATE, pCfg->app.bWinVistaOrLater);
		CtlButn::Enable(hwnd, SUO_C_ASSOCIATE, pCfg->app.bWinVistaOrLater);

		CtlCmbo::Reset(hwnd, SUO_DL_LANGUAGES);
		memcpy(sz, pCfgInst->szExePath, pCfgInst->nExeNameOffs*sizeof(TCHAR));
		_tcscpy(sz + pCfgInst->nExeNameOffs, _T("*.language"));
		
		n = CtlCmbo::AddStr(hwnd, SUO_DL_LANGUAGES, _T("English - US"));
		CtlCmbo::SetSel(hwnd, SUO_DL_LANGUAGES, n);

		ie_UTF8ToUnicode(szCurLanguage, pCfg->app.acLanguage, sizeof(szCurLanguage)/sizeof(TCHAR), sizeof(pCfg->app.acLanguage));

		hff = FindFirstFile(sz, &ffd);
		if (hff != INVALID_HANDLE_VALUE) {
	
			do {
				ffd.cFileName[_tcslen(ffd.cFileName) - _tcslen(_T(".language"))] = 0;
				n = CtlCmbo::AddStr(hwnd, SUO_DL_LANGUAGES, ffd.cFileName);
				if (_tcsicmp(ffd.cFileName, szCurLanguage) == 0)
					CtlCmbo::SetSel(hwnd, SUO_DL_LANGUAGES, n);
			} while (FindNextFile(hff, &ffd));
	
			FindClose(hff);
		}

		return true;

	case WM_COMMAND:
       	switch (LOWORD(wParam)) {

		case 9999:
			SetWindowText(hwnd, _T(PROGRAMNAME " initial setup"));
			CtlText::SetText(hwnd, SUO_T_LANGUAGES, _T("Interface language:"));
			CtlText::SetText(hwnd, SUO_C_ASSOCIATE, _T("Associate supported file types with " PROGRAMNAME "."));
			CtlText::SetText(hwnd, IDOK, _T("Continue >"));
			break;

		case SUO_DL_LANGUAGES:
			if (HIWORD(wParam) != CBN_SELCHANGE) break;
			CtlCmbo::GetText(hwnd, SUO_DL_LANGUAGES, sz, sizeof(sz)/sizeof(TCHAR));
			ie_UnicodeToUTF8(pCfg->app.acLanguage, sz, sizeof(pCfg->app.acLanguage));
			SendMessage(hwnd, WM_COMMAND, 9999, 0);
			pCfgInst->LoadLanguage();
			pCfgInst->SaveDictionary();
			ieTranslateCtrls(hwnd);
			break;

		case IDOK:
			if (CtlButn::GetCheck(hwnd, SUO_C_ASSOCIATE))
				mdShowApplicationAssociationUI(_T(PROGRAMNAME));

			EndDialog(hwnd, true);
			break;
		}
		break;

	case WM_CLOSE:
		EndDialog(hwnd, false);
		break;
	}

	return FALSE;
}


static void ShowSetupOptionsDialog()
// Show setup options dialog
{
	DialogBox(g_hInst, MAKEINTRESOURCE(SETUPDLG), HWND_DESKTOP, &SetupOptionsDlg);
}


//------------------------------------------------------------------------------------------

ConfigInstance::ConfigInstance()
:	hmData(NULL), hmDict(NULL), pDict(nullptr)
{
	pCfgInst = this;

    GetModuleFileName(NULL, szExePath, sizeof(szExePath));
	
	PCTCHAR pcszName, pcszExt;
    ief_SplitPath(szExePath, pcszName, pcszExt);
	nExeNameOffs = pcszName - szExePath;

	SECURITY_ATTRIBUTES  sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = FALSE;  
	
	TCHAR szFileName[MAX_PATH];
	_stprintf(szFileName, _T("Local\\%s Config"), _T(PROGRAMVERSION));

    // Create app shared memory file for config data
	if (!(hmData = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, sizeof(Config), szFileName))) {
		MessageBox(NULL, _T("CreateFileMapping failed!"), _T(PROGRAMNAME), MB_OK);
		ExitProcess(1);
	}
	bool bIsFirst = (GetLastError() != ERROR_ALREADY_EXISTS);

	if (!(pCfg = (Config *)MapViewOfFile(hmData, FILE_MAP_ALL_ACCESS, 0, 0, 0))) {
		MessageBox(NULL, _T("MapViewOfFile failed!"), _T(PROGRAMNAME), MB_OK);
		ExitProcess(1);
	}


	if (bIsFirst) {
		// Init cross-instance global config
		pCfg->ReadFromRegistry();

	} else {
		pCfg->app.iInstances++;

		OpenSharedDictionary();
		if (pDict) g_Language.SelectDictionary(pDict);
	}

	// Set per instance data
	RecentPaths.Init(pCfg->acRecentPaths, sizeof(pCfg->acRecentPaths));

	*szLastFile = 0;
	pCfgInst->RecentPaths.Get(szLastFile);
	if (!*szLastFile) {
		mdGetSpecialFolder(eMyPicturesFolder, szLastFile);
		if (*szLastFile) RecentPaths.Set(szLastFile);
	}

	_tcscpy(szLastSlide, _T("Slide"));
}


ConfigInstance::~ConfigInstance()
{
	if (pCfg) {
		if (!--pCfg->app.iInstances) {
			
			pCfg->WriteToRegistry();
		}

		UnmapViewOfFile(pCfg);
		pCfg = nullptr;
	}

	if (hmData) {
		CloseHandle(hmData);
		hmData = NULL;
	}

	CloseSharedDictionary();

	pCfgInst = nullptr;
}


void ConfigInstance::OpenSharedDictionary()
{
	if (pDict || !pCfg || !pCfg->app.dwDictionarySize) return;

	SECURITY_ATTRIBUTES  sa;
    sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.lpSecurityDescriptor = nullptr;
    sa.bInheritHandle = FALSE;  

	TCHAR szFileName[MAX_PATH], szLanguage[128];
	ie_UTF8ToUnicode(szLanguage, pCfg->app.acLanguage, sizeof(szLanguage)/sizeof(TCHAR), sizeof(pCfg->app.acLanguage));
	_stprintf(szFileName, _T("Local\\%s %s"), _T(PROGRAMVERSION), szLanguage);

    // Create app shared memory file for dictionary data
	if (!(hmDict = CreateFileMapping(INVALID_HANDLE_VALUE, &sa, PAGE_READWRITE, 0, pCfg->app.dwDictionarySize, szFileName))) {
		MessageBox(NULL, _T("CreateFileMapping failed"), _T(PROGRAMNAME), MB_OK);
		ExitProcess(1);
	}

	if (!(pDict = MapViewOfFile(hmDict, FILE_MAP_ALL_ACCESS, 0, 0, 0))) {
		MessageBox(NULL, _T("MapViewOfFile failed"), _T(PROGRAMNAME), MB_OK);
		ExitProcess(1);
	}
}


void ConfigInstance::CloseSharedDictionary()
{
	g_Language.SelectDictionary(nullptr);

	if (pDict) {
		UnmapViewOfFile(pDict);
		pDict = nullptr;
	}

	if (hmDict) {
		CloseHandle(hmDict);
		hmDict = NULL;
	}
}


void ConfigInstance::LoadLanguage()
{
	CloseSharedDictionary();

	TCHAR sz[MAX_PATH];
	
	int n = pCfgInst->nExeNameOffs;
	iem_Copy(sz, pCfgInst->szExePath, n*sizeof(TCHAR));

	ie_UTF8ToUnicode(sz + n, pCfg->app.acLanguage, MAX_PATH - n, sizeof(pCfg->app.acLanguage));

	_tcscat(sz + n, _T(".language"));

	void *pDictionary = g_Language.CompileFile2Dictionary(sz, pCfg->app.dwDictionarySize);
	if (!pDictionary) return;
	
	OpenSharedDictionary();
	
	if (pDict) {
		iem_Copy(pDict, pDictionary, pCfg->app.dwDictionarySize);
		
		g_Language.SelectDictionary(pDict);
	}

	delete[] pDictionary;
}


void ConfigInstance::SaveDictionary()
{
	char szRegPath[MAX_PATH];
	sprintf(szRegPath, "Software\\" PROGCOMPANY "\\%s", PROGRAMNAME);

	mdRegStore rs(HKEY_CURRENT_USER, szRegPath);

	if (pCfg->app.dwDictionarySize && pDict) {
		// Write shared dictionary to registry
		PBYTE pcComp = new BYTE[2*pCfg->app.dwDictionarySize];
		if (!pcComp) return;
		DWORD cbComp = GIFLZW_Compress(pcComp, (const BYTE *)pDict, pCfg->app.dwDictionarySize, 1, 0, 8);
		if (cbComp) rs.SetValue(pcszRegDictionaryName, REG_BINARY, pcComp, cbComp);
		delete[] pcComp;
	} else {
		rs.DeleteValue(pcszRegDictionaryName);
	}
}
