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

#pragma once

#include "mdRecentList.h"


struct Config;
class ConfigInstance;

extern Config *			pCfg;		// Global configuration shared between Image Eye instances (as a memory file)
extern ConfigInstance *	pCfgInst;	// Per instance configuration


//
// Global configuration structs
//

#pragma pack(push)
#pragma pack(4)


struct AppOptions {
	int		iInstances;				// Number of currently open Image Eye instances

	char	acLanguage[64];			// Language selection (in UTF-8)
	DWORD	dwDictionarySize;		// Size of dictionary DB for the selected language translation (0 == no translation)

	bool	bLeaveNoTrace;			// Prevent all writes to the Windows registry?
	bool	bReserved;				// Reserved space for future use
	bool	bWinVistaOrLater;		// Running on Windows Vista or later (winVer.dwMajorVersion >= 6) 
	BYTE	acReserved[5];			// Reserved space for future use
	DWORD	dwReportPos;			// Flags for where on the screen we have report windows open

	bool	bSpawnNewProcess;		// Run app instances as separate processes? (instead of all app windows the same process)
	bool	bSaveRecent;			// Save recent directories
	char	cFolderViewStyle;		// Default folder view style for the file open dialog, i=Icons,l=List,d=Details,n=Thumbnails,t=Tiles
	BYTE	cReserved1;				// Reserved space for future use
};


struct ViewerOptions {
	bool	bHideCaption;			// Hide window caption (when mouse is not over window)
	bool	bRunMaximized;			// Start maximized?
	bool	bXHideProgress;			// Deprecated (Hide progress dialog)
    bool	bAutoSizeImage;			// Auto size image to window
	bool	bAutoSizeOnlyShrink;	// Only shrink (modifies bAutoSizeImage)
    bool	bAutoShrinkInFullscreen;// Auto shrink if larger than the monitor work area in maximized mode
	bool	cReserved1;				// Reserved for future use
	bool	cReserved2;				// Reserved for future use
    bool	bTitleRes;				// Show image resolution in title bar
	bool	bFullPath;				// Show full file path in title bar
	BYTE	acOverlayOpts[3];		// Info overlay 1..3 options (4 bits type (off=0,file name=1,comments=2,date created=3), 2 bits vert pos (top=0,bottom=1), 2 bits horiz pos (top=0,bottom=1,center=2)
	BYTE	acReserved[5];			// Reserved space for future use
	ieBGRA	clrBackground;			// Background color, alpha value is used a a bit-field of flags:
	#define IE_USESOLIDBGCOLOR 0x80	// Always use solid color (no glass effect)
	#define IE_USEFIXEDBGCOLOR 0x40	// Always use fixed color (no auto background)
};


struct SlideOptions {
	DWORD	dwDelayTime10;			// Last delay time set in the CSS dialog (in 1/10ths of a second)
	BYTE	eDelayType;				// Last delay type set in the CSS dialog (none, key, time)
	bool	bRunMaximized;			// Run slide-show in full-screen mode
	bool	bSaveFullPaths;			// Save fully qualified file paths
	bool	bLoop;					// Loop slideshow
	bool	bEditInNotepad;			// Edit script in NotePad before starting it
	bool	bSelfDelete;			// Delete script file when finished
	bool	bSortFiles;				// Sort the files alphabetically?
	BYTE	cReserved;				// Reserved space for future use
};


enum { eArrangeNone, eArrangeByName, eArrangeByImageSize, eArrangeByFileSize, eArrangeByFileDate, eArrangeByPath };

struct IndexOptions {
	bool	bShowDirs;				// Show directories
    bool	bShowOther;				// Show non-image files
	BYTE	cShowNameLines;			// Display image name on this many lines (0 or more)
	bool  	bShowRes;				// Display image file resolutions
	bool	bShowSize;				// Display image file sizes
	bool	bShowDate;				// Display image file dates
	bool	bShowExt;				// Display file extensions
	bool	bShowComment;			// Display comment?
	BYTE	acReserved0[4];			// Reserved space for future use
    SDWORD	iIconX, iIconY;			// Max icon sizes
    SDWORD	iIconSpacing;			// Icon spacing
	SBYTE	cIconArrange;			// Icon sort order
    bool	bCompressCache;			// Compres index chache files?
    bool	bAutoCreateCache;		// Automatically create index chache files?
	bool	bUseSizeFromCache;		// Does icon size stored in index cache override the selected size?
	bool	bFullPathTitle;			// Show the full directory path in the title bar
	bool	bNumFilesTitle;			// Show the number of images in the title bar
	bool	bCloseOnOpen;			// Close index window when opening an image window
	bool	bFreezedWindow;			// Don't auto-size to icons & remember last pos & use for 1st index window
	ieBGRA	clrBackground;			// Background color, A&0x80: Use solid color (no glass effect)
	SWORD	iLastX, iLastY;			// Last window position
	WORD	nLastXW, nLastYH;		// Last window size
};


struct Config {						// All of this is stored persistently as a single data chunk in the registry
public:								// It is shared between all running insances as a (non-disk) memory mapped file
	void ReadFromRegistry();
	void WriteToRegistry();

	// In Options8:
	DWORD			dwCRC32;		// Checksum - used to find if anything of the stored data has changed
	AppOptions		app;
    ViewerOptions	vwr;
    IndexOptions	idx;
	SlideOptions	sli;

	// In Recent:
	BYTE	acRecentPaths[1024];		// Keep track of last used directories...
};


#pragma pack(pop)


//
// Per instance configuration class
//

class ConfigInstance {
friend Config;
public:
	ConfigInstance();
    ~ConfigInstance();

	TCHAR	szExePath[MAX_PATH];	// Program path and file name (and pointers into the same)
	int		nExeNameOffs;			// Program name.exe starts at szProgPath + nProgNameOffs

	typedef enum { eViewerOptions, eIndexOptions, eAboutOptions } eOptionsPage; 
	bool ShowOptionsDialog(HWND hwndParent, eOptionsPage ePage, void *pImageOrIcon = nullptr, bool bInitToImage = false);

	mdRecentList RecentPaths;

	PCTCHAR GetLastFile()						{ return szLastFile; }
	void SetLastFile(PCTCHAR pcszFileName)		{ _tcscpy(szLastFile, pcszFileName); }
	PCTCHAR GetLastSlide()						{ return szLastSlide; }
	void SetLastSlide(PCTCHAR pcszFileName)		{ _tcscpy(szLastSlide, pcszFileName); }

	void LoadLanguage();
	void SaveDictionary();

private:
	TCHAR	szLastFile[MAX_PATH];	// Keep track of last file opened
	TCHAR	szLastSlide[MAX_PATH];	// Keep track of last slide show created

	// Language translation data
	HANDLE	hmData;					// Handle of memory mapped file for shared config data
	HANDLE	hmDict;					// Handle of memory mapped file for shared dictionary data
	void *	pDict;					// Ptry to shared dictionary data

	void OpenSharedDictionary();
	void CloseSharedDictionary();
};
