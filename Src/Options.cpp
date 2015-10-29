//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Options dialog
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

#include <commdlg.h>
#include "ieTextConv.h"
#include "mdSystem.h"
#include "mdDlgCtrls.h"
#include "../Res/resource.h"


//------------------------------------------------------------------------------------------

static void TranslateProp(HWND hwndPage)
{
	// Translare child controls
	ieTranslateCtrls(hwndPage);

	// Ensure topmost
	HWND hwndProp = GetParent(hwndPage);
	if (!hwndProp) return;

	// Get tab window
	HWND hwndTabs = PropSheet_GetTabControl(hwndProp);
	if (!hwndTabs) return;

	// Ensure not multi-line...
	DWORD dwStyle = GetWindowLong(hwndTabs, GWL_STYLE);
	dwStyle &= ~TCS_MULTILINE;
	SetWindowLong(hwndTabs, GWL_STYLE, dwStyle);

	// Translate tabs
	int nMax = TabCtrl_GetItemCount(hwndTabs);

	for (int n = 0; n < nMax; n++) {

		TCITEM item;
		TCHAR sz[256];
		item.mask = TCIF_TEXT;
		item.pszText = sz;
		item.cchTextMax = sizeof(sz)/sizeof(TCHAR);
		if (!TabCtrl_GetItem(hwndTabs, n, &item)) continue;

		PCTCHAR pcszTrans = ieTranslate(sz);
		if (!pcszTrans || !*pcszTrans) continue;

		item.mask = TCIF_TEXT;
		item.pszText = (PTCHAR)pcszTrans;
		TabCtrl_SetItem(hwndTabs, n, &item);
	}
}


static INT_PTR CALLBACK procStdDlg(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message) {

	case WM_INITDIALOG:		
		TranslateProp(hwnd);
		return TRUE;

	case WM_NOTIFY:
		switch (((NMHDR *)lParam)->code) {
		case PSN_APPLY:
			SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
			break;
		case PSN_KILLACTIVE:
			SetWindowLongPtr(hwnd, DWLP_MSGRESULT, FALSE);
			break;
		}
		break;
	}

	return FALSE;
}


ieTextInfoType OverlayType2InfoType(int nOverlayType)
{
	static const ieTextInfoType aTypes[] = { 
		ieTextInfoType::Count, 
		ieTextInfoType::Name, 
		ieTextInfoType::CreationDate, 
		ieTextInfoType::Comment, 
		ieTextInfoType::Author, 
		ieTextInfoType::Page, 
		ieTextInfoType::GpsGeoTag		 
	};
	if ((nOverlayType <= 0) || (nOverlayType > 6)) return ieTextInfoType::Count;
	return aTypes[nOverlayType];
}


static void FillOverlayLists(HWND hwnd, UINT idCtrl, BYTE cSel)
{
	int nSelT = (cSel)>>4;
	int nSelY = (cSel>>2)&3;
	int nSelX = (cSel)&3;

	CtlCmbo::Reset(hwnd, idCtrl);
	for (int t = 0; ; t++) {
		PCTCHAR pcszDesc;
		if (!t) {
			pcszDesc = _T("off");
		} else {
			ieTextInfoType eInfo = OverlayType2InfoType(t);
			if (eInfo == ieTextInfoType::Count) break;
			pcszDesc = ie_DescribeTextInfoType(eInfo);
		}
		int idx = CtlCmbo::AddStr(hwnd, idCtrl, ieTranslate(pcszDesc), t<<4);
		if (t == nSelT) CtlCmbo::SetSel(hwnd, idCtrl, idx);
	}

	idCtrl++;

	CtlCmbo::Reset(hwnd, idCtrl);
	for (int y = 0; y <= 1; y++) {
		for (int x = 0; x <= 2; x++) {
			static PCTCHAR aszY[] = { _T("top"), _T("bottom") };
			static PCTCHAR aszX[] = { _T("left"), _T("right"), _T("center") };
			TCHAR sz[128];
			_stprintf(sz, _T("%s / %s"), ieTranslate(aszY[y]), ieTranslate(aszX[x]));
			int idx = CtlCmbo::AddStr(hwnd, idCtrl, sz, (y<<2)|x);
			if ((y == nSelY) && (x == nSelX)) CtlCmbo::SetSel(hwnd, idCtrl, idx);
		}
	}

	CtlCmbo::Enable(hwnd, idCtrl, nSelT != 0);
}


static COLORREF acrCustClr[16] = { // array of custom colors 
	RGB(0xE0,0xE0,0xE0), RGB(0xA0,0xA0,0xA0), RGB(0xA0,0x00,0x00), RGB(0x00,0xA0,0x00), RGB(0x00,0x00,0xA0), RGB(0xA0,0xA0,0x00), RGB(0xA0,0x00,0xA0), RGB(0x00,0xA0,0xA0),
	RGB(0x60,0x60,0x60), RGB(0x40,0x40,0x40), RGB(0xFF,0x80,0x80), RGB(0x80,0xFF,0x80), RGB(0x80,0x80,0xFF), RGB(0xFF,0xFF,0x80), RGB(0xFF,0x80,0xFF), RGB(0x80,0xFF,0xFF)
};


static INT_PTR CALLBACK ViewerOptionsPage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
//
// Viewer options page
//
{
	static COLORREF crBackground;
	ViewerOptions &vwr = pCfg->vwr;
	int n;

	switch (message) {

	case WM_INITDIALOG:		
		CtlButn::SetCheck(hwnd, PVP_HIDECAPTION, vwr.bHideCaption);
		CtlButn::SetCheck(hwnd, PVP_RUNMAXIMIZED, vwr.bRunMaximized);
    	CtlButn::SetCheck(hwnd, PVP_TITLERES, vwr.bTitleRes);
    	CtlButn::SetCheck(hwnd, PVP_FULLPATH, vwr.bFullPath);
    	CtlButn::SetCheck(hwnd, PVP_AUTOSIZEIMAGE, vwr.bAutoSizeImage);
    	CtlButn::SetCheck(hwnd, PVP_ONLYAUTOSHRINK, vwr.bAutoSizeOnlyShrink);
		CtlButn::Enable(hwnd, PVP_ONLYAUTOSHRINK, vwr.bAutoSizeImage);
		CtlButn::SetCheck(hwnd, PVP_AUTOSHRINKLARGE, vwr.bAutoShrinkInFullscreen);
		for (n = 0; n < 3; n++)
			FillOverlayLists(hwnd, PVP_OVERLAY1TYPE+2*n, vwr.acOverlayOpts[n]);
		CtlButn::SetCheck(hwnd, PVP_GLASSEFFECT, ((vwr.clrBackground.A & IE_USESOLIDBGCOLOR) == 0) && pCfg->app.bWinVistaOrLater);
		CtlButn::Enable(hwnd, PVP_GLASSEFFECT, pCfg->app.bWinVistaOrLater);
		CtlButn::SetCheck(hwnd, PVP_AUTOBGCOLOR, (vwr.clrBackground.A & IE_USEFIXEDBGCOLOR) == 0);
		CtlButn::Enable(hwnd, PVP_BGCOLOR, (vwr.clrBackground.A & IE_USEFIXEDBGCOLOR) != 0);
		crBackground = RGB(vwr.clrBackground.R, vwr.clrBackground.G, vwr.clrBackground.B);
		break;

	case WM_COMMAND:
       	switch (LOWORD(wParam)) {

		case PVP_OVERLAY1TYPE:
		case PVP_OVERLAY2TYPE:
		case PVP_OVERLAY3TYPE:
			if (HIWORD(wParam) != CBN_SELCHANGE) break;
			CtlButn::Enable(hwnd, LOWORD(wParam)+1, CtlCmbo::GetSelData(hwnd, LOWORD(wParam)) != 0);
			break;

		case PVP_AUTOSIZEIMAGE:
			if (HIWORD(wParam) != BN_CLICKED) break;
			CtlButn::Enable(hwnd, PVP_ONLYAUTOSHRINK, CtlButn::GetCheck(hwnd, PVP_AUTOSIZEIMAGE));
			break;

		case PVP_AUTOBGCOLOR:
			if (HIWORD(wParam) != BN_CLICKED) break;
			CtlButn::Enable(hwnd, PVP_BGCOLOR, !CtlButn::GetCheck(hwnd, PVP_AUTOBGCOLOR));
			break;

		case PVP_BGCOLOR: {
			if (HIWORD(wParam) != BN_CLICKED) break;
		
			CHOOSECOLOR cc;
			ZeroMemory(&cc, sizeof(CHOOSECOLOR));
			cc.lStructSize = sizeof(CHOOSECOLOR);
			cc.hwndOwner = hwnd;
			cc.lpCustColors = (LPDWORD)acrCustClr;
			cc.rgbResult = crBackground;
			cc.Flags = CC_RGBINIT | CC_ANYCOLOR | CC_SOLIDCOLOR | CC_FULLOPEN;

			if (ChooseColor(&cc)) {
				crBackground = cc.rgbResult;
			}
		}	break;

		}
		break;

	case WM_NOTIFY:
		switch (((NMHDR *)lParam)->code) {

		case PSN_APPLY:
			vwr.bHideCaption = CtlButn::GetCheck(hwnd, PVP_HIDECAPTION);
    		vwr.bRunMaximized = CtlButn::GetCheck(hwnd, PVP_RUNMAXIMIZED);
    		vwr.bTitleRes = CtlButn::GetCheck(hwnd, PVP_TITLERES);
    		vwr.bFullPath = CtlButn::GetCheck(hwnd, PVP_FULLPATH);
    		vwr.bAutoSizeImage = CtlButn::GetCheck(hwnd, PVP_AUTOSIZEIMAGE);
    		vwr.bAutoSizeOnlyShrink = CtlButn::GetCheck(hwnd, PVP_ONLYAUTOSHRINK);
    		vwr.bAutoShrinkInFullscreen = CtlButn::GetCheck(hwnd, PVP_AUTOSHRINKLARGE);
			vwr.clrBackground = ieBGRA(GetBValue(crBackground), GetGValue(crBackground), GetRValue(crBackground), 0);
			if (!CtlButn::GetCheck(hwnd, PVP_GLASSEFFECT)) 
				vwr.clrBackground.A |= IE_USESOLIDBGCOLOR;
			if (!CtlButn::GetCheck(hwnd, PVP_AUTOBGCOLOR))
				vwr.clrBackground.A |= IE_USEFIXEDBGCOLOR;
			for (n = 0; n < 3; n++)
				vwr.acOverlayOpts[n] = BYTE(CtlCmbo::GetSelData(hwnd, PVP_OVERLAY1TYPE+2*n) | CtlCmbo::GetSelData(hwnd, PVP_OVERLAY1POS+2*n));
			break;
        }
	}

	return procStdDlg(hwnd, message, wParam, lParam);
}


static INT_PTR CALLBACK IndexOptionsPage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
//
// Index options page
//
{
	static COLORREF crBackground;
	IndexOptions &idx = pCfg->idx;

	switch (message) {

	case WM_INITDIALOG:
		TranslateProp(hwnd);

		CtlButn::SetCheck(hwnd, PIP_COMPRESSCACHE, idx.bCompressCache);
		CtlButn::SetCheck(hwnd, PIP_AUTOCREATECACHE, idx.bAutoCreateCache);
		CtlButn::SetCheck(hwnd, PIP_CACHEDSIZEOVERRIDES, idx.bUseSizeFromCache);
		CtlEdit::SetInt(hwnd, PIP_ICONX, idx.iIconX);
    	CtlEdit::SetInt(hwnd, PIP_ICONY, idx.iIconY);
    	CtlEdit::SetInt(hwnd, PIP_ICONSPACING, idx.iIconSpacing);
		CtlButn::SetCheck(hwnd, PIP_FULLPATH, idx.bFullPathTitle);
		CtlButn::SetCheck(hwnd, PIP_NUMFILES, idx.bNumFilesTitle);
		CtlButn::SetCheck(hwnd, PIP_CLOSEONOPEN, idx.bCloseOnOpen);
		CtlButn::SetCheck(hwnd, PIP_GLASSEFFECT, ((idx.clrBackground.A & IE_USESOLIDBGCOLOR) == 0) && pCfg->app.bWinVistaOrLater);
		CtlButn::Enable(hwnd, PIP_GLASSEFFECT, pCfg->app.bWinVistaOrLater);
		crBackground = RGB(idx.clrBackground.R, idx.clrBackground.G, idx.clrBackground.B);
		CtlButn::Enable(hwnd, PIP_BGCOLOR, ((idx.clrBackground.A & IE_USESOLIDBGCOLOR) != 0) || !pCfg->app.bWinVistaOrLater);
		break;

	case WM_COMMAND: 
		switch (LOWORD(wParam)) {

		case PIP_BGCOLOR: {
			if (HIWORD(wParam) != BN_CLICKED) break;
		
			CHOOSECOLOR cc;
			ZeroMemory(&cc, sizeof(CHOOSECOLOR));
			cc.lStructSize = sizeof(CHOOSECOLOR);
			cc.hwndOwner = hwnd;
			cc.lpCustColors = (LPDWORD)acrCustClr;
			cc.rgbResult = crBackground;
			cc.Flags = CC_RGBINIT | CC_ANYCOLOR | CC_SOLIDCOLOR | CC_FULLOPEN;

			if (ChooseColor(&cc)) {
				crBackground = cc.rgbResult;
			}
		}	break;

		case PIP_GLASSEFFECT:
			if (HIWORD(wParam) != BN_CLICKED) break;
			CtlButn::Enable(hwnd, PIP_BGCOLOR, !CtlButn::GetCheck(hwnd, PIP_GLASSEFFECT));
			break;

		}
		break;

	case WM_NOTIFY:
		switch (((NMHDR *)lParam)->code) {
		case PSN_APPLY:
			idx.bCompressCache = CtlButn::GetCheck(hwnd, PIP_COMPRESSCACHE);
			idx.bAutoCreateCache = CtlButn::GetCheck(hwnd, PIP_AUTOCREATECACHE);
			idx.bUseSizeFromCache = CtlButn::GetCheck(hwnd, PIP_CACHEDSIZEOVERRIDES);
	    	idx.iIconX = CtlEdit::GetInt(hwnd, PIP_ICONX);
            if (idx.iIconX < 32) idx.iIconX = 32;
	    	idx.iIconY = CtlEdit::GetInt(hwnd, PIP_ICONY);
            if (idx.iIconY < 32) idx.iIconY = 32;
	    	idx.iIconSpacing = CtlEdit::GetInt(hwnd, PIP_ICONSPACING);
            if (idx.iIconSpacing < 0) idx.iIconSpacing = 0;
			idx.bFullPathTitle = CtlButn::GetCheck(hwnd, PIP_FULLPATH);
			idx.bNumFilesTitle = CtlButn::GetCheck(hwnd, PIP_NUMFILES);
			idx.bCloseOnOpen = CtlButn::GetCheck(hwnd, PIP_CLOSEONOPEN);
			idx.clrBackground = ieBGRA(GetBValue(crBackground), GetGValue(crBackground), GetRValue(crBackground), 0);
			if (!CtlButn::GetCheck(hwnd, PIP_GLASSEFFECT)) 
				idx.clrBackground.A |= IE_USESOLIDBGCOLOR;
			break;
        }
	}

	return procStdDlg(hwnd, message, wParam, lParam);
}


static INT_PTR CALLBACK GeneralOptionsPage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
//
// General options page
//
{
	int n, m;
	TCHAR sz[MAX_PATH], szCurLanguage[128];
	WIN32_FIND_DATA ffd;
	HANDLE hff;

	switch (message) {

	case WM_INITDIALOG:
		TranslateProp(hwnd);

		CtlButn::SetCheck(hwnd, POP_CLEARRECENTONEXIT, pCfg->app.bLeaveNoTrace ||!pCfg->app.bSaveRecent);
		CtlButn::Enable(hwnd, POP_CLEARRECENTNOW, true);

		if (pCfg->app.bLeaveNoTrace) {
			CtlButn::Enable(hwnd, POP_EDITFILETYPES, false);
			CtlButn::Enable(hwnd, POP_CLEARRECENTONEXIT, false);
		}

		CtlCmbo::Reset(hwnd, POP_LANGUAGE);
		memcpy(sz, pCfgInst->szExePath, pCfgInst->nExeNameOffs*sizeof(TCHAR));
		_tcscpy(sz + pCfgInst->nExeNameOffs, _T("*.language"));
		
		n = CtlCmbo::AddStr(hwnd, POP_LANGUAGE, _T("English - US"));
		CtlCmbo::SetSel(hwnd, POP_LANGUAGE, n);

		ie_UTF8ToUnicode(szCurLanguage, pCfg->app.acLanguage, sizeof(szCurLanguage)/sizeof(TCHAR), sizeof(pCfg->app.acLanguage));

		hff = FindFirstFile(sz, &ffd);
		if (hff != INVALID_HANDLE_VALUE) {
	
			do {
				ffd.cFileName[_tcslen(ffd.cFileName) - 9] = 0;
				n = CtlCmbo::AddStr(hwnd, POP_LANGUAGE, ffd.cFileName);

				if (!_tcsicmp(ffd.cFileName, szCurLanguage))
					CtlCmbo::SetSel(hwnd, POP_LANGUAGE, n);

			} while (FindNextFile(hff, &ffd));
	
			FindClose(hff);
		}

		if (!pCfg->app.bWinVistaOrLater) {
			CtlButn::Enable(hwnd, POP_EDITFILETYPES, false);
		}

		break;


	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case POP_CLEARRECENTNOW:
			pCfgInst->RecentPaths.Clear();
			pCfgInst->SetLastFile(_T(""));
			CtlButn::Enable(hwnd, POP_CLEARRECENTNOW, false);
      		break;

		case POP_EDITFILETYPES:
			mdShowApplicationAssociationUI(_T(PROGRAMNAME));
			break;
		}
        break;		

	case WM_NOTIFY:
		switch (((NMHDR *)lParam)->code) {
		case PSN_APPLY:
			pCfg->app.bSaveRecent = !CtlButn::GetCheck(hwnd, POP_CLEARRECENTONEXIT);

			CtlCmbo::GetText(hwnd, POP_LANGUAGE, sz, sizeof(sz)/sizeof(TCHAR));

			ie_UTF8ToUnicode(szCurLanguage, pCfg->app.acLanguage, sizeof(szCurLanguage)/sizeof(TCHAR), sizeof(pCfg->app.acLanguage));
	
			if (_tcsicmp(sz, szCurLanguage) != 0) {
				ie_UnicodeToUTF8(pCfg->app.acLanguage, sz, sizeof(pCfg->app.acLanguage));
				pCfgInst->LoadLanguage();
				pCfgInst->SaveDictionary();
			}
			break;
        }
	}

	return procStdDlg(hwnd, message, wParam, lParam);
}


static INT_PTR CALLBACK ImageInfoOptionsPage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
//
// Image info page
//
{
	TCHAR 			*psz;
	const TCHAR		*pszInfo;
    LPARAM			lp;
	int				i;

	switch (message) {

	case WM_INITDIALOG:
		TranslateProp(hwnd);

		// Image info:
		lp = ((PROPSHEETPAGE *)lParam)->lParam;
		if (!lp) break;

		i = 64;
		CtlList::SetTabStops(hwnd, PAP_INFO, 1, &i);

		iePImageDisplay pimd = (iePImageDisplay)lp;
		if (pimd) for (DWORD dw = 0; dw < DWORD(ieTextInfoType::Count); dw++) {
			
			if (!pimd->Text()->Have(ieTextInfoType(dw))) continue;

			pszInfo = pimd->Text()->Get(ieTextInfoType(dw));
		
			PCTCHAR pcszType = ieTranslate(ie_DescribeTextInfoType(ieTextInfoType(dw)));

			psz = new TCHAR[128 + _tcslen(pszInfo)];
			if (!psz) continue;

			if ((ieTextInfoType(dw) == ieTextInfoType::SourceFile) && (_tcschr(pszInfo, '\\') || _tcschr(pszInfo, '/'))) {
				
				PCTCHAR pcszName, pcszExt;
				ief_SplitPath(pszInfo, pcszName, pcszExt);
				
				if (pcszName > pszInfo) {
				
					_stprintf(psz, _T("%s:\t%s"), pcszType, pcszName);
					CtlList::AddStr(hwnd, PAP_INFO, psz);

					pcszType = ieTranslate(_T("File path"));
					_stprintf(psz, _T("%s:\t"), pcszType);

					int n = pcszName - pszInfo;
					int m = _tcslen(psz);
					memcpy(psz+m, pszInfo, n*sizeof(TCHAR));
					psz[m+n] = 0;

				} else {
					_stprintf(psz, _T("%s:\t%s"), pcszType, pszInfo);
				}

			}
			else if (ieTextInfoType(dw) == ieTextInfoType::SourceBitsPerPixel) {

				_stprintf(psz, _T("%s:\t%dx%dx%s"), pcszType, pimd->X(), pimd->Y(), pszInfo);

			}
			else if (ieTextInfoType(dw) == ieTextInfoType::PixelDensity) {

				iePixelDensity pd;
				ie_ParsePixelDensityInfoStr(pd, pszInfo, ieDotsPerInch);
				_stprintf(psz, _T("%s:\t%dx%d dpi"), pcszType, pd.dwXpU, pd.dwYpU);

			} else {

				_stprintf(psz, _T("%s:\t%s"), pcszType, pszInfo);
			}

			CtlList::AddStr(hwnd, PAP_INFO, psz);

			delete[] psz;
		}

#ifdef _DEBUG
		// For debug compile, output some statistics on the Image info page
		TCHAR sz[MAX_PATH + 32];
		CtlList::AddStr(hwnd, PAP_INFO, _T(""));
		CtlList::AddStr(hwnd, PAP_INFO, _T("--- Memory usage ---"));
		_stprintf(sz, _T("%d bitmaps totalling %d MB"), ieImage::Instances(), int((ieImage::TotalMemoryUsage() + 512*1024) / (1024*1024)));
		CtlList::AddStr(hwnd, PAP_INFO, sz);
#ifdef IE_SUPPORT_IMAGECACHE
		CtlList::AddStr(hwnd, PAP_INFO, _T(""));
		CtlList::AddStr(hwnd, PAP_INFO, _T("--- Image Cache ---"));
		for (int idx = 0;; idx++) {
			int nRefCount;
			PCTCHAR pcsz = g_ieFM.ImageCache.CachedImage(idx, &nRefCount);
			if (!pcsz) break;
			_stprintf(sz, _T("#%d: %s <%d>"), idx, pcsz, nRefCount);
			CtlList::AddStr(hwnd, PAP_INFO, sz);
		}		
#endif
#ifdef IE_SUPPORT_DIRCACHE
		CtlList::AddStr(hwnd, PAP_INFO, _T(""));
		CtlList::AddStr(hwnd, PAP_INFO, _T("--- Directory Cache ---"));
		for (int idx = 0;; idx++) {
			int nRefCount;
			PCTCHAR pcsz = g_ieFM.DirCache.CachedPath(idx, &nRefCount);
			if (!pcsz) break;
			_stprintf(sz, _T("#%d: %s <%d>"), idx, pcsz, nRefCount);
			CtlList::AddStr(hwnd, PAP_INFO, sz);
		}
#endif
#endif
		break;
	}

	return procStdDlg(hwnd, message, wParam, lParam);
}


static INT_PTR CALLBACK AboutAppPage(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
//
// About app page
//
{
	TCHAR s[256];

	switch (message) {

	case WM_INITDIALOG:
		TranslateProp(hwnd);

#ifdef __X64__
		_stprintf(s, _T("%s: v%s\t\tx64  |  %d thread%s  |  %s"), _T(PROGRAMNAME), _T(PROGRAMVERSION), g_nCpuCores, (g_nCpuCores != 1) ? _T("s") : _T(""), g_bSSE41 ? _T("SSE4") : (g_bSSSE3 ? _T("SSSE3") : _T("SSE2")));
		//_stprintf(s, _T("%s: v%s\t\tx64  |  %d thread%s  |  %s"), _T(PROGRAMNAME), _T(PROGRAMVERSION), g_nCpuCores, (g_nCpuCores != 1) ? _T("s") : _T(""), g_bAVX2 ? _T("AVX2") : (g_bAVX ? _T("AVX") : (g_bSSE41 ? _T("SSE4") : (g_bSSSE3 ? _T("SSSE3") : _T("SSE2"))));
#else
		_stprintf(s, _T("%s: v%s\t\tx86  |  %d thread%s  |  %s"), _T(PROGRAMNAME), _T(PROGRAMVERSION), g_nCpuCores, (g_nCpuCores != 1) ? _T("s") : _T(""), g_bSSE41 ? _T("SSE4") : g_bSSSE3 ? _T("SSSE3") : (g_bSSE2 ? _T("SSE2") : (g_bSSE ? _T("SSE") : _T(""))), g_nCpuCores);
		//_stprintf(s, _T("%s: v%s\t\tx86  |  %d thread%s  |  %s"), _T(PROGRAMNAME), _T(PROGRAMVERSION), g_nCpuCores, (g_nCpuCores != 1) ? _T("s") : _T(""), g_bAVX2 ? _T("AVX2") : (g_bAVX ? _T("AVX") : (g_bSSE41 ? _T("SSE4") : g_bSSSE3 ? _T("SSSE3") : (g_bSSE2 ? _T("SSE2") : (g_bSSE ? _T("SSE") : _T(""))))));
#endif
		CtlItem::SetText(hwnd, PAP_IEYE, s);

#ifdef IE_SUPPORT_IMAGECACHE
#ifdef _DEBUG
		_stprintf(s, _T("Images: %d / %.2f MB  |  Cached : %d / %.2f MB  |  Memory: %.2f MB"), ieImage::Instances(), float(ieImage::TotalMemoryUsage()) / float(1024.0f*1024.0f), g_ieFM.ImageCache.Size(), float(g_ieFM.ImageCache.MemoryUsage()) / float(1024.0f*1024.0f), float(iem_AvailMemory()) / float(1024.0f*1024.0f));
		CtlItem::SetText(hwnd, PAP_IDEBUG, s);
#endif
#endif
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
        case PAP_WWW:
        case PAP_DONATE:
			mdSpawnURL((LOWORD(wParam) == PAP_WWW) ? "http://www.fmjsoft.com/" : "http://www.fmjsoft.com/imageeye.html#buyit");
      		break;
		}
        break;
	case WM_NOTIFY:
		switch (((NMHDR *)lParam)->code) {
		case PSN_APPLY:
			SetWindowLongPtr(hwnd, DWLP_MSGRESULT, PSNRET_NOERROR);
			break;
        }
        break;
	}

	return procStdDlg(hwnd, message, wParam, lParam);
}


bool ConfigInstance::ShowOptionsDialog(HWND hwndParent, eOptionsPage ePage, void *pImageOrIcon, bool bInitToImage)
//
// Properties dialog
//
{
	PROPSHEETPAGE	pp[5];
	struct {
		bool bAllModes;
		PCTCHAR pcszTemplate;
		DLGPROC pfnDlgProc;
	} DlgPages[] = {
		{ true, MAKEINTRESOURCE(PVPDLG), &ViewerOptionsPage },
		{ true, MAKEINTRESOURCE(PIPDLG), &IndexOptionsPage },
		{ true, MAKEINTRESOURCE(POPDLG), &GeneralOptionsPage },
		{ false,MAKEINTRESOURCE(PAPDLG), &ImageInfoOptionsPage },
		{ true, MAKEINTRESOURCE(PGPDLG), &AboutAppPage },
		{ false, nullptr, nullptr }
	};

	int nPages = 0, j;
	for (j = 0; DlgPages[j].pcszTemplate; j++) {
		if (!DlgPages[j].bAllModes && (ePage != eViewerOptions)) continue;
		pp[nPages].dwSize = sizeof(PROPSHEETPAGE);
		pp[nPages].hInstance = g_hInst;
		pp[nPages].pszTemplate = DlgPages[j].pcszTemplate;
		pp[nPages].pfnDlgProc = DlgPages[j].pfnDlgProc;
		pp[nPages].dwFlags = 0;
		pp[nPages].lParam = (LPARAM)pImageOrIcon;
		nPages++;
	}

	TCHAR szCaption[256];
	_stprintf(szCaption, _T("%s | %s"), ieTranslate(_T("Options")), _T(PROGRAMVERNAME));

    PROPSHEETHEADER	ps;
	ps.dwSize = sizeof(PROPSHEETHEADER);
	ps.dwFlags = PSH_USEICONID | PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW | PSH_NOCONTEXTHELP;
	ps.hwndParent = hwndParent;
	ps.hInstance = g_hInst;
	ps.pszIcon = MAKEINTRESOURCE(OPTIONSICO);
	ps.pszCaption = szCaption;
	ps.nPages = nPages;
	ps.nStartPage = (ePage == eIndexOptions) ? 1 : ((bInitToImage && pImageOrIcon) ? 3 : 0);
	ps.ppsp = pp;

	SetCursor(LoadCursor(NULL, IDC_WAIT)); // Slow machines takes a while to open the properties...

	if (PropertySheet(&ps) < 0) exit(1);

    return true;
}
