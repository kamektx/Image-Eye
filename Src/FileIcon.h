//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	File icons (thumbnails) and collections of file icons
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

#include "muiMain.h"
#include "ieDirectoryCache.h"


//------------------------------------------------------------------------------------------

class FileIcon;
class FileIconCache;
class FileIconCollection;

enum FileIconType {
	fitUndecided,
	fitDirectory,
    fitImage,
    fitSlideshow,
    fitDocument
};



//------------------------------------------------------------------------------------------

class IFileIconDrawing {
public:
	virtual ~IFileIconDrawing() {}
	typedef enum { eClear, eDefault, eSelected, eFocused, ePressed } eColorList;
	typedef enum { eFileName, eComment, eFileInfo } eTextColor;

	virtual void Fill(HDC hdc, const RECT &rc, eColorList eColor = eClear) = 0;
	virtual void Frame(HDC hdc, const RECT &rc, eColorList eColor = eDefault) = 0;
	virtual void Text(HDC hdc, PCTCHAR pcszText, const RECT &rc, eTextColor eColor, UINT uFlags = 0) = 0;
	virtual int GetLengthOfText(HDC hdc, PCTCHAR pcszText, int nTextChars = -1) = 0;
	int GetCharHeight() const { return iCharYH; }
	virtual ieBGRA GetColor(eColorList eColor) = 0;

protected:
	IFileIconDrawing() : iCharYH(0) {}
	int iCharYH;
};

extern IFileIconDrawing *NewFileIconDrawNormal(muiFont &ft);
extern IFileIconDrawing *NewFileIconDrawGlass(muiFont &ft);


//------------------------------------------------------------------------------------------

class ieString {
public:
	ieString() : pcszStr(_T("")), nLength(0), pszAlloc(nullptr) {}
	ieString(PCTCHAR pcszInitStr, bool bStoreAsReference = false) : pcszStr(nullptr), nLength(0), pszAlloc(nullptr) { Set(pcszInitStr, bStoreAsReference); }
	virtual ~ieString() { if (pszAlloc) delete[] pszAlloc; }

	void Set(PCTCHAR pcszNewStr, bool bStoreAsReference = false);
	PCTCHAR Str() const { return pcszStr; }
	int Length() const { return nLength; }

protected:
	PCTCHAR pcszStr;
	int nLength;
	PTCHAR pszAlloc;
};

class ieFileString : public ieString {
public:
	ieFileString(PCTCHAR pcszInitFileStr = nullptr) : ieString() { Set(pcszInitFileStr); }
	virtual ~ieFileString() {}

	bool Set(PCTCHAR pcszNewFileStr);
	bool SetPath(PCTCHAR pcszNewPath);
	bool SetNameAndExt(PCTCHAR pcszNewName);

	PCTCHAR NameStr() const { return pcszName; }
	PCTCHAR ExtStr() const { return pcszExt; }
	int PathLength() const { return int(pcszName - szFile); }
	int NameWithoutExtLength() const { return int(pcszExt - pcszName); }
	int NameAndExtLength() const { return int(Length() - PathLength()); }

	bool IsExt(PCTCHAR pcszTestExt) const;

protected:
	TCHAR szFile[MAX_PATH];
	PCTCHAR pcszName, pcszExt;
};


//------------------------------------------------------------------------------------------

class FileIcon {
//
// A single thumbnail icon representing a file
//
friend FileIconCollection;
public:
	FileIcon(PCTCHAR pcszFile, QWORD qwFileSize, IE_FILETIME ftFileTime, bool bIsDirectory);
    ~FileIcon();
    
	void Draw(HDC hdc, IFileIconDrawing *pDraw, int iScrollY, int iWindowYH);
	bool HitTest(muiCoord xyTest);
	
	FileIcon *NextIcon() const { return pNext; }
	FileIcon *PrevIcon() const { return pPrev; }

	FileIconType GetType() const { return Type; }
	void SetType(FileIconType Type_) { Type = Type_; }

	PCTCHAR GetFileStr() const { return FileInfo.File.Str(); }
	PCTCHAR GetFileNameStr() const { return FileInfo.File.NameStr(); }
	PCTCHAR GetFileExtStr() const { return FileInfo.File.ExtStr(); }
	bool IsFileExt(PCTCHAR pcszTestExt) const { return FileInfo.File.IsExt(pcszTestExt); }
	QWORD GetFileSize() const { return FileInfo.qwSize; }
	PCTCHAR GetFileSizeStr() const { return FileInfo.szFileSize; }
	IE_FILETIME GetFileTime() const { return FileInfo.ftTimeStamp; }
	PCTCHAR GetFileTimeStr() const { return FileInfo.szFileTime; }
	PCTCHAR GetCommentStr() const { return ImageInfo.Comment.Str(); }
	int GetFileStrLength() const { return FileInfo.File.Length(); }
	int GetFileNameWithoutExtStrLength() const { return FileInfo.File.NameWithoutExtLength(); }
	int GetFileNameAndExtStrLength() const { return FileInfo.File.NameAndExtLength(); }
	void SetFileStr(PCTCHAR pcszStr) { FileInfo.File.Set(pcszStr); }
	void SetFileSize(QWORD qwFileSize);
	void SetFileTime(IE_FILETIME ftFileTime);

	int GetImageDimensionX() const { return ImageInfo.iX; }
	int GetImageDimensionY() const { return ImageInfo.iY; }
	int GetImageDimensionZ() const { return ImageInfo.iZ; }
	PCTCHAR GetDimensionStr() const { return ImageInfo.szDimensions; }
	int GetCommentStrLength() const { return ImageInfo.Comment.Length(); }
	bool HasImage() const { return ImageInfo.pimd != nullptr; }
	iePImageDisplay GetImage() const { return ImageInfo.pimd; }
	void SetImageDimensions(int iX, int iY, int iZ);
	void SetComment(PCTCHAR pcszComment) { ImageInfo.Comment.Set(pcszComment); }
	void SetImage(iePImageDisplay pimd) { ImageInfo.pimd = pimd; }

	void SetStateSelected(bool bSelected)	{ IconState.bSelected = bSelected; }
	void SetStateFocused(bool bFocused)		{ IconState.bFocused = bFocused; }
	void SetStatePressed(bool bPressed)		{ IconState.bPressed = bPressed; }
	void SetStateClearAll()				{ IconState.bPressed = IconState.bFocused = IconState.bSelected = false; }
	bool IsStateSelected() const		{ return IconState.bSelected; }
	bool IsStateFocused() const			{ return IconState.bFocused; }
	bool IsStatePressed() const			{ return IconState.bPressed; }

	void SetPlacement(muiCoord xyPos, muiSize whSize, muiSize whMaxIcon) { IconPlacement.bVisible = true; IconPlacement.xyPos = xyPos; IconPlacement.whSize = whSize; IconPlacement.whMaxIcon = whMaxIcon;	}
	void SetPlacementHidden() { IconPlacement.bVisible = false; }
	bool IsPlacementVisible() const { return IconPlacement.bVisible; }
	void GetPlacementPos(muiCoord &xyPos) const { xyPos = IconPlacement.xyPos;	}
	void GetPlacementSize(muiSize &whSize) const { whSize = IconPlacement.whSize;	}

    void ShellOpenFile(HWND hwnd) const;

private:
	FileIcon		*pPrev, *pNext;
	FileIconType	Type;

	struct TFileInfo {
		TFileInfo(PCTCHAR pcszFile) : File(pcszFile), qwSize(0), ftTimeStamp(0) { *szFileTime = 0; *szFileSize = 0; }
		ieFileString	File;
		QWORD			qwSize;
		IE_FILETIME		ftTimeStamp;
		TCHAR			szFileTime[22], szFileSize[10];
	};

	struct TImageInfo {
		TImageInfo() : Comment(), pimd(nullptr), iX(0), iY(0), iZ(0), hWinIcon(NULL), bTriedWinIcon(false) { *szDimensions = 0; }
		ieString		Comment;
	    iePImageDisplay	pimd;
	    int				iX, iY, iZ;
		TCHAR			szDimensions[18];
   		HICON			hWinIcon;
		bool			bTriedWinIcon;
	};

	struct TIconPlacement {
		TIconPlacement() : bVisible(false) {}
		bool			bVisible;
		muiCoord		xyPos;
		muiSize			whSize;
		muiSize			whMaxIcon;
	};

	struct TIconState {
		TIconState() : bSelected(false), bFocused(false), bPressed(false) {}
		bool			bSelected, bFocused, bPressed;
	};

	TFileInfo FileInfo;
	TImageInfo ImageInfo;
	TIconPlacement IconPlacement;
	TIconState IconState;

	void UnlinkIcon() { if (pPrev) pPrev->pNext = pNext; if (pNext) pNext->pPrev = pPrev; pPrev = pNext = nullptr; }
	void LinkIcon(FileIcon *pPrevLink, FileIcon *pNextLink) { pPrev = pPrevLink; pNext = pNextLink; if (pPrev) pPrev->pNext = this; if (pNext) pNext->pPrev = this; }

	void GetIconWH(muiSize &whIcon, bool &bCenterIcon);
	void DrawLongText(HDC hdc, IFileIconDrawing *pDraw, RECT &rc, UINT uTextFormat, IFileIconDrawing::eTextColor clr, PCTCHAR pcszText, int nTextLen, int &nMaxLines, bool bAddLinkSymbol) const;
};

typedef FileIcon *PFileIcon;


//------------------------------------------------------------------------------------------

class FileIconCache {
//
// An on-disk cache of precomputed icon images
//
public:
	FileIconCache();
	~FileIconCache();

    bool Load(PCTCHAR pcszPath, muiSize &whIcon);	// Load cache file into memory, set iIconX and/or iIconY to the required icon size (will return false if cache has other size), or set to 0 if accepting the size stored in the cache file (which is then return in iIconX/iIconY)
    bool Save(FileIcon *pFirstIcon, muiSize whIcon, HWND hwndNotify = NULL, UINT uMsgNotify=0, WPARAM wParamNotify=0, LPARAM lParamNotify = 0);	// Save cache file from pIcon chain
	bool RetrieveIcon(FileIcon *pIcon);				// Retrieve an icon image from the cache
    void Remove();									// Remove cache file (if present)
    void Free();									// Free cached data
	bool HaveCache() const { return bHaveCache; }	// Is cached icon images present?
	bool HaveFile() const { return HaveCache() || (strCacheFile.Length() && ief_Exists(strCacheFile.Str())); }	// Is cached file present? (differs from HaveCache() in that the latter returns false e.g. if the cache file has icons of wrong size)
	bool IsDirty() const { return bHaveCache && bUpdateCache; }	// Does the cache contain up-to-date icons for all images?
	void Invalidate() { bUpdateCache = true; }		// Invalidate cached data (i.e. tell it that it no longer contains up-to-date icons for all images)

private:
	ieFileString strCacheFile;	// Cache file path+name
	PBYTE pDATA, pFREF;			// Raw (but uncompressed) data from cache file
    int iNumCacheRefs;			// Number of entries in pFREF
	int iSizeOfFREF;			// Size of entries in pFREF (not including file name)
	int iFmtIconX, iFmtIconY;	// Bounding size of icons in cache
	bool bHaveCache;			// Does strCacheFile exist?
	bool bUpdateCache;			// Does strCacheFile need to be updated?
	bool bFileNamesAreUTF8;		// Are file names in the cache stored as UTF-8 encoded Unicode? (otherwise they're in Latin-1)
	bool bMetaDataIncluded;		// Does the cache include meta data (comment) strings?
};


//------------------------------------------------------------------------------------------

class FileIconCollection {
//
// A collection of file icon images
//
public:
	FileIconCollection();
	~FileIconCollection();

	typedef int IconSortFunc(FileIcon *pIcon1, FileIcon *pIcon2);
    static IconSortFunc ByPath;
    static IconSortFunc ByName;
    static IconSortFunc ByImageSize;
    static IconSortFunc ByFileSize;
    static IconSortFunc ByFileDate;

	int ReadDirectory(ieIDirectoryEnumerator *&pEnum, PCTCHAR pcszPath, muiSize whIcon = { 0, 0 }, IconSortFunc *pSortFunc = nullptr);	// Create file icons for a directory, iIconX|iIconY=0 indicates that the icon size stored in the cache (if any) should be used instead of the size from the global index options, returns the total number of icons
	int ReadSubDirectories(IconSortFunc *pSortFunc = nullptr);												// Recursively parse sub-directories of the already read directory, and create icons for them
	void ReadImagesAsync(HWND hwndNotify, UINT uMsgNotify, WPARAM wParamNotify);						// Read in image icons - works asynchronously
	FileIcon *AddFile(PCTCHAR pcszNewName, bool bReadIcon = true);	// Add a single file to the icon list
	void FreeIcons();												// Free all file icons
	int ArrangeIcons(int iMaxXW, int iCharYH, muiSize &whUsed);			// Arrange icons to fit inside a given window width, returns the number of visible (non-hidden) icons
	void CancelRead() { if (bReadingImages) { bCancelReads = true; tg.cancel(); tg.wait(); } }

	FileIcon *FirstIcon() const { return pFirstIcon; }				// Retrieve the first of the file icons, call FileIcon::NextIcon() for the next one, and so on...
	bool IncludesSubDirectories() const { return bInclSubDirs; }	// Does the icon collection include icons for sub-directories? (read by ReadSubDirectories())

	const muiSize &IconSize() const { return whIcon; }
	int IconX() const { return whIcon.w; }							// Width of icon images
	int IconY() const { return whIcon.h; }							// Height of icon images

	const muiSize &GridSize() const { return whGrid; }
	int GridX() const { return whGrid.w; }							// Width of icon + spacing
	int GridY() const { return whGrid.h; }							// Height of icon + text + spacing

	void DeleteIcon(FileIcon *pfi);									// Delete a file icon
	void RenameIcon(FileIcon *pfi, PCTCHAR pcszNewName);			// Rename a file icon

	void Sort(IconSortFunc *pSortFunc);								// Sort the icons, use functions below as argument

	void SaveCache(HWND hwndNotify=NULL, UINT uMsgNotify=0, WPARAM wParamNotify=0, LPARAM lParamNotify=0);
	bool HaveCache() const { return Cache.HaveCache(); }
	bool HaveCacheFile() const { return Cache.HaveFile(); }
	bool IsCacheDirty() const { return Cache.IsDirty(); }
	void RemoveCache() { Cache.Remove(); }
	void FreeCache() { Cache.Free(); }

private:
	TCHAR szPath[MAX_PATH];				// Directory path
	bool bInclSubDirs;					// Can't save index, e.g. when recursed sub-dirs...

	muiSize whIcon;						// Icon bounding dimensions
	FileIcon *pFirstIcon, *pLastIcon;	// Chain of icons

	muiSize whGrid;						// Icon grid settings
	int iGridMaxXW;

	task_group	tg;						// Task group, for reading image icons & for reading & saving cache
	bool bReadingImages;				// True after a task to read images has been launched, and until it's been wait'ed on
	bool bSavingCache;					// true after a save cache task has been launcehd, and until it's been wait'ed on
	volatile bool bCancelReads;			// Set true to cancel ongoing reads...

	FileIconCache Cache;				// Icon image file cache

	FileIcon *AddIcon(PCTCHAR pcszFile, QWORD qwSize_, IE_FILETIME ftTimeStamp_, bool bIsDirectory);
	void ReadIcon(FileIcon *pfi, HWND hwndNotify = NULL, UINT uMsgNotify = 0, WPARAM wParamNotify = 0);	// Create the icon image for an icon

	// Merge sort helpers
	void MergeSort(FileIcon *&headRef);
	FileIcon *SortedMerge(FileIcon *a, FileIcon *b);
	void FrontBackSplit(FileIcon *source, FileIcon *&frontRef, FileIcon *&backRef);
	IconSortFunc *pMergeComp;
};
