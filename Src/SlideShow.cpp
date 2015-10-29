//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Slide shows
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

#include "ieTextConv.h"
#include "mdSystem.h"
#include <shellapi.h>
#include "mdDlgCtrls.h"
#include "EyeScriptParser.h"
#include "../Res/resource.h"


//-----------------------------------------------------------------------------
// .ies- Image Eye slideshow script file format definition
//-----------------------------------------------------------------------------
/*
ImageEye SlideShow {
	encoding x;				// Text encoding, x=1252|UTF8 (default=1252)
	delay nsec;				// Wait nsec seconds, nsec=key => wait for keypress
	display "imagefile";	// Show this image file
	loop n { ... }			// Loop section n times, no-n => infinite
	shell "exename param";
	shell "verb" "documentname";
	set SelfDelete;
	set AutoSizeImage bool;
}

Note: The text may be encoded either using the Windows 1252 code page (extended ISO 8859-1), or as UTF-8 encoded Unicode.
*/
//------------------------------------------------------------------------------------------
// End of .ies defitions
//------------------------------------------------------------------------------------------


static bool PumpMessageQueueAndCheckKeyPress(bool &bQuit, bool bPaused = false)
{
   	MSG	msg;
	msg.message = 0;

	while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {

		TranslateMessage(&msg);

		// We grab Esc and Pause keys...
		switch (msg.message) {

		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			switch (msg.wParam) {
			case VK_ESCAPE:
			case VK_PAUSE:
				continue;
			default:
				return true;
			}
			break;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			switch (msg.wParam) {
			case VK_ESCAPE:
				bQuit = true;
				return true;
			case VK_PAUSE:
				if (!bPaused) while (!bQuit) {
					if (PumpMessageQueueAndCheckKeyPress(bQuit, true))
						break;
				}
				return true;
			default:
				continue;
			}
			break;

		case WM_LBUTTONDOWN:
		case WM_MBUTTONDOWN:
		case WM_QUIT:
			DispatchMessage(&msg);
			return true;

		case WM_USER:
			switch (msg.wParam) {
			case 30272:
				PostMessage(msg.hwnd, WM_COMMAND, CMD_EXIT, LPARAM(msg.hwnd));
			case 30271:
				bQuit = true;
				return true;
			}
			break;

		default:
			DispatchMessage(&msg);
		}
	}

	Sleep(0);

	return false;
}


//-----------------------------------------------------------------------------

static bool ParseBlock(ieIViewer &Viewer, EyeScriptParser &Parser, bool &bSelfDelete, bool bCacheAhead = false)
// Parse a block { ... } of statements, return false if user wants to quit, else true
{
    int		i;
    bool	bMore, bQuit = false;
	char	s[MAX_PATH+64], t[MAX_PATH];
	HWND	hwnd = Viewer.GetHWnd();

    // Fetch command tokens
	for (;;) {
		if (!Parser.ParseToken(s, &bMore)) break;
        // Now have a command token:

        if (*s == '}') {
        	return true;

        } else if (*s == '{') {
        	if (!ParseBlock(Viewer, Parser, bSelfDelete, bCacheAhead)) return false;
            bMore = false;

        } else if (strcmp(s, "delay") == 0) {
            if (!bMore) continue;
			if (!bCacheAhead) {
				Parser.ParseToken(s, &bMore);
				if (strcmp(s, "key") == 0) {
					for (; !bQuit && !PumpMessageQueueAndCheckKeyPress(bQuit););
				} else {
					i = int(atoi(s)*10.0 + 0.5);
			        while (i--) {
						if (bQuit || PumpMessageQueueAndCheckKeyPress(bQuit))
							break;
            			Sleep(100);
					}
	            }
				if (bQuit) return false;
            }

        } else if (strcmp(s, "display") == 0) {

			if (!bMore) continue;

			Parser.ParseToken(s, &bMore);

			TCHAR szFile[MAX_PATH];
#ifdef UNICODE
			ie_Latin1ToUnicode(szFile, s, sizeof(szFile)/sizeof(TCHAR));
			if (!ief_Exists(szFile)) {
				ie_UTF8ToUnicode(szFile, s, sizeof(szFile)/sizeof(TCHAR));
			}
#else
			strcpy(szFile, s);
			if (!f_Exists(szFile)) {
				wchar_t wszFile[MAX_PATH];
				mdUTF8ToUnicode(wszFile, s, sizeof(wszFile)/sizeof(wchar_t));
				mdUnicodeToLatin1(szFile, wszFile, sizeof(szFile));
			}
#endif

			if (!bCacheAhead) {
	            HCURSOR hCurOld = SetCursor(LoadCursor(NULL, IDC_WAIT));
				
				// Load image
				if (!Viewer.LoadNewImage(szFile, false, false)) {
					return false;
				}

				// Cache next image
				DWORD dwPos = Parser.GetParsePoint();
				ParseBlock(Viewer, Parser, bSelfDelete, true);
				Parser.SetParsePoint(dwPos);

				SetCursor(hCurOld);

			} else {
#ifdef IE_SUPPORT_IMAGECACHE
				// Pre-cache this one, then return to 'real' position
				g_ieFM.ImageCache.CacheImageAsync(szFile, ieSearchDirection::ThisFile, false);
#endif
				return false;
			}

        } else if (strcmp(s, "loop") == 0) {

            if (!bMore) continue;
            i = -1;
			if (Parser.ParseToken(s, &bMore)) {
				while (*s != '{') {
					if ((*s >= '0') && (*s <= '9'))
						i = atoi(s);
            		if (!Parser.ParseToken(s)) 
						break;
				}
				if (*s != '{') continue;
				DWORD dwPos = Parser.GetParsePoint();
				for (;;) {
					if ((i >= 0) && (i-- == 0)) break;
					Parser.SetParsePoint(dwPos);
	            	if (!ParseBlock(Viewer, Parser, bSelfDelete)) return false;
				}
            }

        } else if (strcmp(s, "set") == 0) {
            if (!bMore) continue;
			if (!bCacheAhead) {
	        	Parser.ParseToken(s, &bMore);
				if (_strcmpi(s, "SelfDelete") == 0)
					bSelfDelete = true;
		        if (bMore) {
					if (_strcmpi(s, "AutoSizeImage") == 0)
				    	Parser.ParseBool(pCfg->vwr.bAutoSizeImage, &bMore);
				}
			}

        } else if (strcmp(s, "shell") == 0) {
            if (!bMore) continue;
			if (!bCacheAhead) {

				Parser.ParseToken(s, &bMore);

				if (bMore) {	// Ex: shell open "hi.wav";
					strcpy(t, s);
					Parser.ParseToken(s, &bMore);
				} else {
					*t = 0;
				}
#ifdef UNICODE
				TCHAR ws[MAX_PATH+64], wt[MAX_PATH];
				ie_Latin1ToUnicode(ws, s, sizeof(ws)/sizeof(TCHAR));
				if (!ief_Exists(ws)) {
					ie_UTF8ToUnicode(ws, s, sizeof(ws)/sizeof(TCHAR));
				}
				if (*t) ie_Latin1ToUnicode(wt, t, sizeof(wt)/sizeof(TCHAR));
				ShellExecute(hwnd, *t ? wt : NULL, ws, NULL, NULL, SW_SHOW);
#else
				ShellExecute(hwnd, *t ? t : NULL, s, NULL, NULL, SW_SHOW);
#endif
			}
        }

        // Skip any additional command parameters
        while (bMore) {
			if (!Parser.ParseToken(s, &bMore)) break;
        }

        // Keep window responding to user input
		PumpMessageQueueAndCheckKeyPress(bQuit);
        if (bQuit || !IsWindow(hwnd)) return false; // Quit if the window has been closed
    }

    return true;
}


//-----------------------------------------------------------------------------

bool RunSlideShow(ieIViewer &Viewer, PCTCHAR pcszScriptFile)
{
	IE_HFILE hf = ief_Open(pcszScriptFile);
    if (hf == IE_INVALIDHFILE) return false;;

	HWND hwnd = Viewer.GetHWnd();

	bool bRestoreWindow = false;
	if (pCfg->sli.bRunMaximized) {
		bRestoreWindow = !IsZoomed(hwnd);
		PostMessage(hwnd, WM_COMMAND, CMD_GOFULLSCREEN, LPARAM(hwnd));
	}

	PostMessage(hwnd, WM_COMMAND, CMD_ONRUNNINGSLIDESHOW | (1<<16), LPARAM(hwnd));

	PCTCHAR pcszName, pcszExt;
	ief_SplitPath(pcszScriptFile, pcszName, pcszExt);
	int nPathLen = (pcszName - pcszScriptFile);

	TCHAR szPath[MAX_PATH];
	memcpy(szPath, pcszScriptFile, nPathLen*sizeof(TCHAR));
	szPath[nPathLen] = 0;
	SetCurrentDirectory(szPath);

    DWORD	dwSize = ief_Size(hf);
    PBYTE	pbSlide = new BYTE[dwSize+1];

	ief_Read(hf, pbSlide, dwSize);

	pbSlide[dwSize] = 0;

	ief_Close(hf);
	

	EyeScriptParser Parser(pbSlide, pbSlide + dwSize, dwSize);

    bool bOldAutoSizeImage = pCfg->vwr.bAutoSizeImage;
	bool bOldAutoSizeOnlyShrink = pCfg->vwr.bAutoSizeOnlyShrink;
    bool bOldAutoShrinkLarge = pCfg->vwr.bAutoShrinkInFullscreen;
    pCfg->vwr.bAutoSizeImage = true;
	pCfg->vwr.bAutoSizeOnlyShrink = true;
    pCfg->vwr.bAutoShrinkInFullscreen = true;

    bool	bMore;
	char	s[MAX_PATH];

	Parser.ParseToken(s, &bMore);

	if (_stricmp(s, "ImageEye") != 0) bMore = false;
    if (bMore) Parser.ParseToken(s, &bMore);
    if (!bMore || (_strcmpi(s, "SlideShow") != 0)) {
    	delete[] pbSlide;
		return false;
    }

	do {
		Parser.ParseToken(s, &bMore);
    } while (bMore && (_strcmpi(s, "{") != 0));

	bool bDeleteFile = false;
    ParseBlock(Viewer, Parser, bDeleteFile);

    delete[] pbSlide;

	if (bDeleteFile) ief_Delete(pcszScriptFile);

	PostMessage(hwnd, WM_COMMAND, CMD_ONRUNNINGSLIDESHOW | (0 << 16), LPARAM(hwnd));

	if (bRestoreWindow) PostMessage(hwnd, WM_COMMAND, CMD_GOWINDOWED, LPARAM(hwnd));

    pCfg->vwr.bAutoSizeImage = bOldAutoSizeImage;
	pCfg->vwr.bAutoSizeOnlyShrink = bOldAutoSizeOnlyShrink;
	pCfg->vwr.bAutoShrinkInFullscreen = bOldAutoShrinkLarge;

	return true;
}


//-----------------------------------------------------------------------------

// Create slide show script default settings
enum { eNoDelay, eKeyPress, eDelay };


static INT_PTR CALLBACK CreateSlideScriptDlg(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TCHAR szSlideName[MAX_PATH];

	switch (uMsg) {

	case WM_INITDIALOG:
		ieTranslateCtrls(hwnd);
		
		// Set small icon
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM) (HICON) LoadImage(g_hInst, MAKEINTRESOURCE(SLIDEICO), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));
		
		_stprintf(szSlideName, _T("%s | %s"), ieTranslate(_T("Create slideshow script")), _T(PROGRAMVERNAME));
		SetWindowText(hwnd, szSlideName);

		// Init controls
		CtlEdit::SetText(hwnd, CSS_FILENAME, pCfgInst->GetLastSlide());
		CtlEdit::SetFInt1(hwnd, CSS_DELAYTIME, pCfg->sli.dwDelayTime10);
		CtlButn::SetCheck(hwnd, CSS_NODELAY, pCfg->sli.eDelayType == eNoDelay);
		CtlButn::SetCheck(hwnd, CSS_KEYPRESS, pCfg->sli.eDelayType == eKeyPress);
		CtlButn::SetCheck(hwnd, CSS_DELAY, pCfg->sli.eDelayType == eDelay);
		CtlButn::SetCheck(hwnd, CSS_SORTFILES, pCfg->sli.bSortFiles);
		CtlButn::SetCheck(hwnd, CSS_FULLPATHS, pCfg->sli.bSaveFullPaths);
		CtlButn::SetCheck(hwnd, CSS_LOOP, pCfg->sli.bLoop);
		CtlButn::SetCheck(hwnd, CSS_NOTEPAD, pCfg->sli.bEditInNotepad);
		CtlButn::SetCheck(hwnd, CSS_SELFDELETE, pCfg->sli.bSelfDelete);
		return 1;

	case WM_COMMAND:

		switch (LOWORD(wParam)) {

		case CSS_DELAYTIME:
			if (HIWORD(wParam) != EN_CHANGE) break;
			CtlButn::Click(hwnd, CSS_DELAY);
			CtlEdit::SetFocus(hwnd, CSS_DELAYTIME);
			break;

		case IDOK:
			CtlEdit::GetText(hwnd, CSS_FILENAME, szSlideName, sizeof(szSlideName)/sizeof(TCHAR));
			if (*szSlideName) pCfgInst->SetLastSlide(szSlideName);
			if (CtlButn::GetCheck(hwnd, CSS_NODELAY)) pCfg->sli.eDelayType = eNoDelay;
			if (CtlButn::GetCheck(hwnd, CSS_KEYPRESS)) pCfg->sli.eDelayType = eKeyPress;
			if (CtlButn::GetCheck(hwnd, CSS_DELAY)) pCfg->sli.eDelayType = eDelay;
			pCfg->sli.dwDelayTime10 = CtlEdit::GetFInt1(hwnd, CSS_DELAYTIME);
			if (pCfg->sli.dwDelayTime10 < 0) pCfg->sli.dwDelayTime10 = 0;
			pCfg->sli.bSortFiles = CtlButn::GetCheck(hwnd, CSS_SORTFILES);
			pCfg->sli.bSaveFullPaths = CtlButn::GetCheck(hwnd, CSS_FULLPATHS);
			pCfg->sli.bLoop = CtlButn::GetCheck(hwnd, CSS_LOOP);
			pCfg->sli.bEditInNotepad = CtlButn::GetCheck(hwnd, CSS_NOTEPAD);
			pCfg->sli.bSelfDelete = CtlButn::GetCheck(hwnd, CSS_SELFDELETE);
			EndDialog(hwnd, 1);
			break;

		case IDCANCEL:
			EndDialog(hwnd, 0);
			break;
		}
		return 1;

	case WM_CLOSE:
		PostMessage(hwnd, WM_COMMAND, IDCANCEL, 0);
		return 1;
	}

	return FALSE;
}


static void WriteLn(IE_HFILE hf, const char *s)
{
	static const char NL[2] = { 13, 10 };
	ief_Write(hf, s, (DWORD)strlen(s));
	ief_Write(hf, NL, 2);
}


bool CreateSlideScript(PTCHAR pszScriptFile, HWND hwndDlgParent, PCTCHAR pcszDefaultDir, bool bAutoOptions)
{
#ifndef IE_SUPPORT_DIRCACHE
	return false;
#else
	if (!pszScriptFile) return false;

	bool bPrevSortFiles, bPrevSelfDelete, bPrevSaveFullPaths, bPrevEditInNotepad;
	if (bAutoOptions) {
		bPrevSortFiles = pCfg->sli.bSortFiles;
		bPrevSelfDelete = pCfg->sli.bSelfDelete;
		bPrevSaveFullPaths = pCfg->sli.bSaveFullPaths;
		bPrevEditInNotepad = pCfg->sli.bEditInNotepad;
		pCfg->sli.bSortFiles = true;
		pCfg->sli.bSelfDelete = true;
		pCfg->sli.bSaveFullPaths = false;
		pCfg->sli.bEditInNotepad = false;
	}
	else {
		if (!DialogBox(g_hInst, MAKEINTRESOURCE(CSSDLG), hwndDlgParent, &CreateSlideScriptDlg))
			return false;
	}
	
	// Get file enumerator

	if (pcszDefaultDir && *pcszDefaultDir) {
		_tcscpy(pszScriptFile, pcszDefaultDir);
		SetCurrentDirectory(pszScriptFile);
	} else {
		GetCurrentDirectory(MAX_PATH, pszScriptFile);
	}

	ieIDirectoryEnumerator *pEnum = g_ieFM.DirCache.CreateEnumerator(pszScriptFile);
	if (!pEnum) return false;

	_stprintf(pszScriptFile + _tcslen(pszScriptFile), _T("\\%s.ies"), pCfgInst->GetLastSlide());
	
	// Create script file

	IE_HFILE hf = ief_Open(pszScriptFile, ieCreateNewFile);
    if (hf == INVALID_HANDLE_VALUE) {
		GetTempPath(MAX_PATH, pszScriptFile);
		_tcscat(pszScriptFile, _T("\\ieslide.ies"));
		hf = ief_Open(pszScriptFile, ieCreateNewFile);
	    if (hf == INVALID_HANDLE_VALUE) {
			MessageBox(hwndDlgParent, ieTranslate(_T("Couldn't create script file!")), _T(PROGRAMNAME), MB_OK);
			return false;
		}
    }

	WriteLn(hf, "ImageEye SlideShow {");

	if (pCfg->sli.bLoop) WriteLn(hf, "loop {");

	if (pCfg->sli.bSelfDelete) WriteLn(hf, "\tset SelfDelete;");


	PCTCHAR pcszFile;
	const ieDirectoryFileInfo *pDFI;

	// Count image files

#ifdef UNICODE
	bool bUseUTF8 = false;
	DWORD nNumEntries = 0;
#endif
	for (pcszFile = nullptr; (pcszFile = pEnum->NextFile(pcszFile, &pDFI)) != nullptr;) {
		if (pDFI->bSubDirectory || (g_ieFM.FindFileReaderFromExtension(pcszFile) < 0)) continue;
#ifdef UNICODE
		if (!bUseUTF8 && !ie_IsUnicode2Latin1Lossless(pcszFile)) {
			bUseUTF8 = true;
		}
#endif
		nNumEntries++;
	}

	// Create array of file names

	PCTCHAR *ppFileNames = new PCTCHAR[nNumEntries];
	if (!ppFileNames) return false;

	nNumEntries = 0;

	for (pcszFile = nullptr; (pcszFile = pEnum->NextFile(pcszFile, &pDFI)) != nullptr;) {
		if (pDFI->bSubDirectory || (g_ieFM.FindFileReaderFromExtension(pcszFile) < 0)) continue;
		ppFileNames[nNumEntries++] = pcszFile;
	}

	if (pCfg->sli.bSortFiles) {
		
		// Bubble-sort

		DWORD n = nNumEntries;
		do {
			DWORD newn = 0;
			for (DWORD i = 1; i < n; i++) {
				if (ief_StrCmpOS(ppFileNames[i - 1], ppFileNames[i]) > 0) {
					PCTCHAR t = ppFileNames[i - 1];
					ppFileNames[i - 1] = ppFileNames[i];
					ppFileNames[i] = t;
					newn = i;
				}
			}
			n = newn;
		} while (n);
	}

	// Write file entries in script file

	for (DWORD n = 0; n < nNumEntries; n++) {
		
		PCTCHAR p = pcszFile = ppFileNames[n];
		
		if (!pCfg->sli.bSaveFullPaths) {
			p += _tcslen(p);
	        while ((p >= pcszFile) && (*p != '\\') && (*p != '/') && (*p != ':')) p--;
			p++;
		}

		char sz[3*MAX_PATH+128];
        strcpy(sz, "\tdisplay \"");
#ifdef UNICODE
		if (bUseUTF8) ie_UnicodeToUTF8(sz+strlen(sz), p, 3*MAX_PATH);
		else ie_UnicodeToLatin1(sz+strlen(sz), p, 3*MAX_PATH);
#else
		strcat(sz, p);
#endif
		strcat(sz, "\";");

		if (pCfg->sli.eDelayType == eKeyPress) strcat(sz, " delay key;");
		if (pCfg->sli.eDelayType == eDelay) sprintf(sz+strlen(sz), " delay %.1f;", pCfg->sli.dwDelayTime10/10.0f);
		WriteLn(hf, sz);
    }

	// Finish up

	if (pCfg->sli.bLoop) WriteLn(hf, "} // loop");

	WriteLn(hf, "}");

    ief_Close(hf);

	pEnum->Release();

	delete[] ppFileNames;

	if (pCfg->sli.bEditInNotepad) mdNotepad(pszScriptFile, true);

	if (bAutoOptions) {
		pCfg->sli.bSortFiles = bPrevSortFiles;
		pCfg->sli.bSelfDelete = bPrevSelfDelete;
		pCfg->sli.bSaveFullPaths = bPrevSaveFullPaths;
		pCfg->sli.bEditInNotepad = bPrevEditInNotepad;
	}

	return true;
#endif //IE_SUPPORT_DIRCACHE
}
