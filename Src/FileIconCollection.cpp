//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	A collection of file icon images
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

#include "mdSystem.h"
#include "FileIcon.h"


//------------------------------------------------------------------------------------------

FileIconCollection::FileIconCollection()
	: pFirstIcon(nullptr), pLastIcon(nullptr), Cache(),
	bReadingImages(false), bSavingCache(false), bCancelReads(false),
	whIcon(pCfg->idx.iIconX, pCfg->idx.iIconY), bInclSubDirs(false),
	iGridMaxXW(0), whGrid(1, 1)
{
	*szPath = 0;
}


FileIconCollection::~FileIconCollection()
{
	tg.wait();
	FreeIcons();
}


int FileIconCollection::ReadDirectory(ieIDirectoryEnumerator *&pEnum, PCTCHAR pcszPath, muiSize whIcon_, IconSortFunc *pSortFunc)
{
	if (bReadingImages || bSavingCache) {
		if (bReadingImages) {
			bCancelReads = true;
			tg.cancel();
		}
		tg.wait();
		bReadingImages = bSavingCache = false;
	}

	bool bGotPath = pcszPath && *pcszPath;
	bool bSamePath = bGotPath && (_tcsicmp(pcszPath, szPath) == 0);
	
	FileIcon *pOldIcons = nullptr;
	if (pFirstIcon && bSamePath) {
		// Same path -> Save a copy previous icons that we can retrieve the new icons from for all files that hasn't changed
		pOldIcons = pFirstIcon;
		pFirstIcon = pLastIcon = nullptr;
	} else {
		FreeIcons();
	}

	if (!bGotPath) return 0;

	// Store new path
	int nPathLen = _tcslen(pcszPath);
	memcpy(szPath, pcszPath, (nPathLen+1)*sizeof(*pcszPath));

	bInclSubDirs = false;
	bCancelReads = false;

	// Read icon cache file (if present)
	tg.run([&] { Cache.Load(szPath, whIcon_); });

    // Create a list of icons for all files in the directory
	int iNumIcons = 0;

	for (;;) {	// (repeat until we've got a list where the directory hasn't changed during the enumeration)

		ieIDirectoryEnumerator *pOldEnum = pEnum;
		pEnum = g_ieFM.DirCache.CreateEnumerator(szPath);
		if (pOldEnum) pOldEnum->Release();

		const ieDirectoryFileInfo *pDFI;	

		if (((nPathLen == 2) || (nPathLen == 3)) && (szPath[1] == ':')) {

			// Special case for root path: then add drive letters
			int iCurDrive = _totupper(szPath[0]) - 'A';
			DWORD dwDriveMap = GetLogicalDrives();
			TCHAR szFile[4] = _T("X:\\");

			for (int i = 32; i--; ) {
			
				if (!(dwDriveMap & (1<<i))) continue;
				if (i == iCurDrive) continue;
				szFile[0] = 'A' + i;

				AddIcon(szFile, 0, 0, true);
				iNumIcons++;
			}
		}

		if (!pEnum) break;	// No files?

		// Enumerate files

		for (PCTCHAR pcszName = nullptr; pEnum->IsStillValid() && (pcszName = pEnum->NextFile(pcszName, &pDFI)) != nullptr;) {
	
			// Ignore hidden files
			if (pDFI->bHiddenFile) continue;

			// Ignore . directory
			int nNameLen = _tcslen(pcszName);
			if ((nNameLen == 1) && (pcszName[0] == '.') && (pcszName[1] == 0))
				continue;

			// Ignore .iei and .iea files
			if (!pDFI->bSubDirectory) {
	   			if ((nNameLen > 4) && (pcszName[nNameLen-4] == '.') && ((pcszName[nNameLen-3] == 'i') || (pcszName[nNameLen-3] == 'I')) && ((pcszName[nNameLen-2] == 'e') || (pcszName[nNameLen-2] == 'E')) && ((pcszName[nNameLen-1] == 'i') || (pcszName[nNameLen-1] == 'I') || (pcszName[nNameLen-1] == 'A') || (pcszName[nNameLen-1] == 'A')))
					continue;
			}

			// Expand to full file path
			TCHAR szFile[MAX_PATH];
			int nFileLen = nPathLen;
			memcpy(szFile, szPath, nFileLen*sizeof(*szPath));
			szFile[nFileLen++] = '\\';
			memcpy(szFile+nFileLen, pcszName, (nNameLen+1)*sizeof(*pcszName));
			nFileLen += nNameLen;

			// See if we can re-use any old icon...
			FileIcon *pThis = pOldIcons;

			while (pThis) {
				if (	(pThis->GetFileSize() == pDFI->qwSize)							// Must have same file size
					&&	(pThis->GetFileTime() == pDFI->ftFileTime)						// ... and time stampe
					&&	(pThis->GetFileStrLength() == nFileLen)							// ... and length
					&&	!memcmp(pThis->GetFileStr(), szFile, nFileLen*sizeof(*szFile))	// ... and name
					&&	((pThis->GetType() != fitImage) || pThis->HasImage()) )			// ... and if it's an image, we must've read the image thumbnail...
					break;
				pThis = pThis->NextIcon();
			}

			if (pThis) {
		
				// Yes, unlink from old chain and re-use it in the new chain
				if (pThis == pOldIcons) pOldIcons = pThis->NextIcon();
				pThis->UnlinkIcon();

				if (!pFirstIcon) pFirstIcon = pThis;
				pThis->LinkIcon(pLastIcon, nullptr);
				pLastIcon = pThis;

			} else {

				// No, so create new icon
				AddIcon(szFile, pDFI->qwSize, pDFI->ftFileTime, pDFI->bSubDirectory);
			}

			iNumIcons++;
		}

		if (pEnum->IsStillValid()) {
			// Success!
			break;
		}

		// Directory has changed during the enumeration -> Try again (save any icons read as old icons that can be re-used)
		if (pFirstIcon) {
			pLastIcon->LinkIcon(pLastIcon->PrevIcon(), pOldIcons);
			pOldIcons = pFirstIcon;
		} 
		pFirstIcon = pLastIcon = nullptr;
		iNumIcons = 0;
	}

	// Free any remaining old icons
	for (;;) {
		FileIcon *pThis = pOldIcons;
		if (!pThis) break;
		pOldIcons = pThis->NextIcon();
		delete pThis;
	}

	// Sort new icons?
	if (pSortFunc && (pSortFunc != ByImageSize)) {
		Sort(pSortFunc);
	}

	// Wait for cache file to load
	tg.wait();

	whIcon = whIcon_;
	if (!whIcon.w) whIcon.w = pCfg->idx.iIconX;
	if (!whIcon.h) whIcon.h = pCfg->idx.iIconY;

	// Return the number of FileIcon's created
	return iNumIcons;
}


int FileIconCollection::ReadSubDirectories(IconSortFunc *pSortFunc)
{
	if (bReadingImages || bSavingCache) {
		tg.wait();
		bReadingImages = bSavingCache = false;
	}

	if (!pFirstIcon || bInclSubDirs) return 0;

	FileIcon *pThis = pFirstIcon;
	int iNumIcons = 0;
	do {
		iNumIcons++;
		pThis = pThis->NextIcon();
	} while (pThis);

	FileIconCollection *pficSubDir = new FileIconCollection();

	for (pThis = pFirstIcon; pThis; pThis = pThis->NextIcon()) {

		if (pThis->GetType() != fitDirectory) continue;

		PCTCHAR pcszName = pThis->GetFileNameStr();
		if (*pcszName == '.') continue;
		if (pcszName[0] && (pcszName[1] == ':') && !pcszName[3]) continue;

		ieIDirectoryEnumerator *pEnum = nullptr;

		int iSubIcons = pficSubDir->ReadDirectory(pEnum, pThis->GetFileStr(), whIcon, pSortFunc);
		if (iSubIcons > 0) bInclSubDirs = true;

		if (pEnum) pEnum->Release();

		if (pficSubDir->pFirstIcon) {

			FileIcon *pPrev = pThis;
			FileIcon *pNext = pficSubDir->pFirstIcon;
			pficSubDir->pFirstIcon = pficSubDir->pLastIcon = nullptr;

			while (pNext) {

				FileIcon *pAdd = pNext;
				pNext = pAdd->NextIcon();
				pAdd->UnlinkIcon();

				if (*pAdd->GetFileNameStr() == '.') {					
					delete pAdd;
					continue;
				}

				pAdd->LinkIcon(pPrev, pPrev->NextIcon());
				pPrev = pAdd;
				iNumIcons++;
			}
		}
	}

	delete pficSubDir;

	if (pLastIcon) for (;;) {
		FileIcon *pNext = pLastIcon->NextIcon();
		if (!pNext) break;
		pLastIcon = pNext;
	}

	if (bInclSubDirs) {
		AddFile(szPath, false);
		iNumIcons++;
	}

	return iNumIcons;
}


void FileIconCollection::ReadImagesAsync(HWND hwndNotify, UINT uMsgNotify, WPARAM wParamNotify)
{
	bReadingImages = true;

	for (FileIcon *pEnum = pFirstIcon; pEnum; pEnum = pEnum->NextIcon()) {
		tg.run([=] { ReadIcon(pEnum, hwndNotify, uMsgNotify, wParamNotify); } );
	}
}


void FileIconCollection::ReadIcon(FileIcon *pIcon, HWND hwndNotify, UINT uMsgNotify, WPARAM wParamNotify)
{
	iePFileReader pifr = nullptr;
	iePImage pim = nullptr;

	for (;;) {

		if (bCancelReads || pIcon->HasImage()) break;

	    // Try the image icon cache
		if (Cache.RetrieveIcon(pIcon))
			break;
		
		// Try reading 'undecided' files as an image file!
		if (pIcon->GetType() != fitUndecided) break;

		// Resolve shell links
		TCHAR szBuf[MAX_PATH];
		PCTCHAR pcszFile = pIcon->GetFileStr();
		if (pIcon->IsFileExt(_T(".lnk"))) {
			if (mdResolveShellLink(szBuf, pcszFile, false)) {
				pcszFile = szBuf;
				WIN32_FIND_DATA fd;
				HANDLE hff = FindFirstFile(pcszFile, &fd);
				if (hff != INVALID_HANDLE_VALUE) {
					FindClose(hff);
					pIcon->SetFileSize(fd.nFileSizeLow);
					pIcon->SetFileTime(*(QWORD *)&fd.ftLastWriteTime);
					if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
						pIcon->SetType(fitDirectory);
						break;
					}
				}
			}
		}

		if (ieFailed(g_ieFM.CreateFileReader(pifr, pcszFile, false))) {			
			// Not a recognized image file
			pIcon->SetType(fitDocument);
			break;
		}

		// Got an image file (that wasn't in the cache)
		pIcon->SetImageDimensions(pifr->X(), pifr->Y(), pifr->Z());
		Cache.Invalidate();
		if (bCancelReads) break;

		// Decode it into an image
		bool bDecodeSmall = pifr->HaveSmall() && ((pifr->SmallX() >= whIcon.w) || (pifr->SmallY() >= whIcon.h));

		g_ieFM.CreateImage(pifr, pim, false, bDecodeSmall);
		if (!pim) break;

		if (ieFailed(pifr->ReadImage(pim, false, nullptr, &bCancelReads)) || bCancelReads) break;

		// Copy comment or longest keyword to icon
		PCTCHAR pcszComment = nullptr;
		if (pim->Text()->Have(ieTextInfoType::Comment)) {
			pcszComment = pim->Text()->Get(ieTextInfoType::Comment);
		} else {
			int nMaxLen = 0;
			for (int n = 0; n < 9; n++) {
				if (!pim->Text()->Have(ieTextInfoType(int(ieTextInfoType::Keyword1) + n))) continue;
				PCTCHAR pcszKeyword = pim->Text()->Get(ieTextInfoType(int(ieTextInfoType::Keyword1) + n));
				int nKeywordLen = _tcslen(pcszKeyword);
				if (nKeywordLen > nMaxLen) {
					nMaxLen = nKeywordLen;
					pcszComment = pcszKeyword;
				}
			}					
		}
		if (pcszComment && *pcszComment) pIcon->SetComment(pcszComment);
		if (bCancelReads) break;

		// Create display image wrapper for the large image
		iePImageDisplay pimd = ieImageDisplay::Create(pim);
		if (!pimd) break;
		pim = nullptr;

		pimd->SetRequiresDraweable(false);
		pimd->SetDisplayBitDepth(32);
		pimd->PrepareAdju();
		pimd->Adjustments()->SetZoomToFitInside(pimd->WH(), { ieDist(whIcon.w), ieDist(whIcon.h) });
		pimd->PrepareDisp();
		if (bCancelReads) break;

		if (pimd->X()*pIcon->GetImageDimensionY() != pIcon->GetImageDimensionX()*pimd->Y()) {
			// Image has been rotated +/- 90 deg
			pIcon->SetImageDimensions(pIcon->GetImageDimensionY(), pIcon->GetImageDimensionX(), pIcon->GetImageDimensionZ());
		}

		// Create a BGRA image copy of the downsized display image, i.e. the icon
		iePImage pimIcon = pimd->DispImage()->CreateCopy(false, true, false, iePixelFormat::BGRA);
		pimd->Release();
		if (!pimIcon || bCancelReads) break;

		// To save memory, don't store any text meta data in the icons image
		for (int n = 0; n < int(ieTextInfoType::Count); n++)
			pimIcon->Text()->Set(ieTextInfoType(n), nullptr);

		// Create display image wrapper for the icon image
		pIcon->SetImage(ieImageDisplay::Create(pimIcon));

		break;
	}

	if (pim) pim->Release();
	if (pifr) pifr->Release();

	// Last resort if we didn't get any image above is to ask windows for an icon
	// ... but this has to wait until it is draw ... 
	// ... since both ExtractAssociatedIcon() and SHGetFileInfo(..., SHGFI_ICON) will return the wrong icons ...
	// ... if not called from the main thread!!! 
	// NB: Delaying it also has the benefit of not having to read the icons if "!bShowOthers"

	if (hwndNotify && uMsgNotify && !bCancelReads) {
		// Report ReadIcon() completed
		PostMessage(hwndNotify, uMsgNotify, wParamNotify, (LPARAM)pIcon);
	}
}


void FileIconCollection::FreeIcons()
{
	if (bReadingImages || bSavingCache) {
		if (bReadingImages) tg.cancel();
		tg.wait();
		bReadingImages = bSavingCache = false;
	}

	Cache.Free();

	if (!pFirstIcon) return;

	FileIcon *pNext = pFirstIcon;
	pFirstIcon = pLastIcon = nullptr;

	for (;;) {
    	FileIcon *pThis = pNext;
		if (!pThis) break;
		pNext = pThis->NextIcon();
		delete pThis;
    }
}


void FileIconCollection::MergeSort(FileIcon *&headRef)
// sorts the linked list by changing next pointers (not data)
{
	// Base case -- length 0 or 1
	if (!headRef || !headRef->pNext) return;

	// Split head into 'a' and 'b' sublists
	FileIcon *a, *b;
	FrontBackSplit(headRef, a, b);

	// Recursively sort the sublists
	MergeSort(a);
	MergeSort(b);

	// Merge the two sorted lists together
	headRef = SortedMerge(a, b);
}

FileIcon *FileIconCollection::SortedMerge(FileIcon *a, FileIcon *b)
{
	FileIcon *head = nullptr, *tail;	// Head & current tail of new list being constructed by merging the sorted lists a & b

	for (;;) {
		// Only a or b left?
		if (!a) {
			if (!head) {
				head = b;
			} else if (b) {
				tail->pNext = b;
				b->pPrev = tail;
			}
			return head;
		}
		else if (!b) {
			if (!head) {
				head = a;
			} else if (a) {
				tail->pNext = a;
				a->pPrev = tail;
			}
			return head;
		}

		// Compare a with b
		int i;
		bool bAIsDir = (a->GetType() == fitDirectory);
		bool bBIsDir = (b->GetType() == fitDirectory);

		if ((bAIsDir != bBIsDir) && (pMergeComp != FileIconCollection::ByPath)) {
			i = bAIsDir ? -1 : 1;	// Sort directories before files
		}
		else if (bAIsDir && bBIsDir) {
			if ((a->GetFileStr()[3] == 0) && (b->GetFileStr()[3] != 0)) i = -1;	// Sort drive roots before other directories
			else if ((b->GetFileStr()[3] == 0) && (a->GetFileStr()[3] != 0)) i = 1;
			else i = pMergeComp(a, b);
		}
		else { // Sort normal files
			i = pMergeComp(a, b);
		}

		// Pick a or b & advance the picked list, and make the picked item the new tail
		FileIcon *x;
		if (i <= 0) {
			x = a;
			a = a->pNext;
		}
		else {
			x = b;
			b = b->pNext;
		}
		if (!head) {
			head = x;
		}
		else {
			tail->pNext = x;
			x->pPrev = tail;
		}
		tail = x;
	}
}


void FileIconCollection::FrontBackSplit(FileIcon *source, FileIcon *&frontRef, FileIcon *&backRef)
{
	if (!source || !source->pNext) {
		// length <= 1
		frontRef = source;
		backRef = nullptr;
		return;
	}

	// Advance 'fast' two nodes, and advance 'slow' one node
	FileIcon *slow = source;
	FileIcon *fast = source->pNext;
	while (fast) {
		fast = fast->pNext;
		if (fast) {
			slow = slow->pNext;
			fast = fast->pNext;
		}
	}

	// 'slow' is before the midpoint in the list, so split it in two at that point.
	frontRef = source;
	backRef = slow->pNext;
	slow->pNext->pPrev = nullptr;
	slow->pNext = nullptr;
}


void FileIconCollection::Sort(IconSortFunc *pSortFunc)
{
	if (bSavingCache) {
		tg.wait();
		bSavingCache = false;
	}

	pMergeComp = pSortFunc;

	if ((pMergeComp == ByPath) && !IncludesSubDirectories()) {
		pMergeComp = ByName;
	}

	MergeSort(pFirstIcon);

	if (pLastIcon) while (pLastIcon->pNext) {
		pLastIcon = pLastIcon->pNext;
	}
}


int FileIconCollection::ByPath(FileIcon *pIcon1, FileIcon *pIcon2)
{
	PCTCHAR pcsz1 = pIcon1->GetFileStr(), pcsz2 = pIcon2->GetFileStr();
	PCTCHAR pcszN1 = pIcon1->GetFileNameStr(), pcszN2 = pIcon2->GetFileNameStr();
	int iPathLen1 = pcszN1 - pcsz1, iPathLen2 = pcszN2 - pcsz2;

	if (pIcon1->GetType() == fitDirectory) {
		if (*pcszN1 == '.') return -1;
		iPathLen1 += _tcslen(pcszN1);
	}

	if (pIcon2->GetType() == fitDirectory) {
		if (*pcszN2 == '.') return 1;
		iPathLen2 += _tcslen(pcszN2);
	}

	int i = ief_StrCmpOS(pcsz1, pcsz2, iPathLen1, iPathLen2);
	if (i != 0) return i;

	return ief_StrCmpOS(pcszN1, pcszN2);
}


int FileIconCollection::ByName(FileIcon *pIcon1, FileIcon *pIcon2)
{
	return ief_StrCmpOS(pIcon1->GetFileNameStr(), pIcon2->GetFileNameStr(), pIcon1->GetFileNameAndExtStrLength(), pIcon2->GetFileNameAndExtStrLength(), true);
}


int FileIconCollection::ByImageSize(FileIcon *pIcon1, FileIcon *pIcon2)
{
	bool bIsImage1 = (pIcon1->GetType() == fitImage);
	bool bIsImage2 = (pIcon2->GetType() == fitImage);

	if (bIsImage1 != bIsImage2) {
		// One of the icons is not an image
		return bIsImage1 ? -1 : 1;
	} else if (!bIsImage1) {
		// Neither icon is an image
		return ByName(pIcon1, pIcon2);
	}

	// Both icons are images
	DWORD dwSize1 = pIcon1->GetImageDimensionX() * pIcon1->GetImageDimensionY();
	DWORD dwSize2 = pIcon2->GetImageDimensionX() * pIcon2->GetImageDimensionY();
	
	if (dwSize1 < dwSize2) return -1;
	if (dwSize1 > dwSize2) return 1;
	
	if (pIcon1->GetImageDimensionZ() < pIcon2->GetImageDimensionZ()) return -1;
	if (pIcon1->GetImageDimensionZ() > pIcon2->GetImageDimensionZ()) return 1;

    return 0;
}


int FileIconCollection::ByFileSize(FileIcon *pIcon1, FileIcon *pIcon2)
{
	if (pIcon1->GetFileSize() < pIcon2->GetFileSize()) return -1;
	if (pIcon1->GetFileSize() > pIcon2->GetFileSize()) return 1;

	return 0;
}


int FileIconCollection::ByFileDate(FileIcon *pIcon1, FileIcon *pIcon2)
{
	if (pIcon1->GetFileTime() < pIcon2->GetFileTime()) return -1;
	if (pIcon1->GetFileTime() > pIcon2->GetFileTime()) return 1;

	return 0;
}


void FileIconCollection::DeleteIcon(FileIcon *pfi)
{
	if (bReadingImages || bSavingCache) {
		tg.wait();
		bReadingImages = bSavingCache = false;
	}

	if (pfi->GetType() == fitImage) Cache.Invalidate();

	if (pfi == pFirstIcon) pFirstIcon = pfi->NextIcon();
	
	delete pfi;
}


void FileIconCollection::RenameIcon(FileIcon *pfi, PCTCHAR pcszNewName)
{
	if (bReadingImages || bSavingCache) {
		tg.wait();
		bReadingImages = bSavingCache = false;
	}

	if (pfi->GetType() == fitImage) Cache.Invalidate();

	pfi->SetFileStr(pcszNewName);
}


FileIcon *FileIconCollection::AddFile(PCTCHAR pcszNewName, bool bReadIcon)
{
	WIN32_FIND_DATA	ffd;
	HANDLE hff = FindFirstFile(pcszNewName, &ffd);
	if (hff == INVALID_HANDLE_VALUE) return nullptr;

	FindClose(hff);

	FileIcon *pfi = AddIcon(pcszNewName, ffd.nFileSizeLow, *(QWORD *)&ffd.ftLastWriteTime, ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY);
	if (!pfi) return nullptr;

	if (bReadIcon) ReadIcon(pfi);

	if (pfi->GetType() == fitImage) Cache.Invalidate();

	return pfi;
}


FileIcon *FileIconCollection::AddIcon(PCTCHAR pcszFile, QWORD qwSize_, IE_FILETIME ftTimeStamp_, bool bIsDirectory)
{
	// Add icon
	FileIcon *pfi = new FileIcon(pcszFile, qwSize_, ftTimeStamp_, bIsDirectory);
	if (!pfi) return nullptr;
	
	if (!pFirstIcon) pFirstIcon = pfi;

	pfi->LinkIcon(pLastIcon, nullptr);

	pLastIcon = pfi;
	
	return pfi;
}


int FileIconCollection::ArrangeIcons(int iMaxXW, int iCharYH, muiSize &whUsed)
{
	whUsed = { 0, 0 };

	FileIcon *pEnum = FirstIcon();
	if (!pEnum) return 0;

	int iIconSpacing = pCfg->idx.iIconSpacing;
	if (iCharYH) {
		whGrid.h = iIconSpacing + whIcon.h + 2;
		int iGridYwoText = whGrid.h;

		if (pCfg->idx.cShowNameLines)	whGrid.h += iCharYH * pCfg->idx.cShowNameLines;
		if (pCfg->idx.bShowComment)		whGrid.h += iCharYH + 5;
		if (pCfg->idx.bShowRes)			whGrid.h += iCharYH;
		if (pCfg->idx.bShowSize)		whGrid.h += iCharYH;
		if (pCfg->idx.bShowDate)		whGrid.h += iCharYH;

		if (whGrid.h != iGridYwoText)	whGrid.h += iCharYH/5;

		whGrid.w = whIcon.w + iIconSpacing + 5;
	}
	if (iMaxXW) {
		iGridMaxXW = iMaxXW;
	} else {
		iMaxXW = iGridMaxXW;
	}

	int nBorder = 2 + iIconSpacing;
	muiCoord xy(nBorder, nBorder);
	int nVisible = 0;

	if (xy.x + whGrid.w > iMaxXW) xy.y -= whGrid.h;
	
	for (; pEnum; pEnum = pEnum->NextIcon()) {

		bool bVisible;
		switch (pEnum->GetType()) {
		case fitImage:
		case fitUndecided:
			bVisible = true;
			break;
		case fitDirectory:
			bVisible = pCfg->idx.bShowDirs;
			break;
		default:
			bVisible = pCfg->idx.bShowOther;
			break;
		}

		if (!bVisible) {
			pEnum->SetPlacementHidden();
			continue;
		}

		nVisible++;

		if ((xy.x + whGrid.w) > iMaxXW) {
			xy.x = nBorder;
			xy.y += whGrid.h;
		}

		pEnum->SetPlacement(xy, whGrid, whIcon);

		xy.x += whGrid.w;
		if (xy.x > whUsed.w) whUsed.w = xy.x;
	}

	xy.y += whGrid.h;

	if (whUsed.w > iGridMaxXW) whUsed.w = iGridMaxXW;
	whUsed.h = xy.y + (iIconSpacing / 2) + 2;

	return nVisible;
}


void FileIconCollection::SaveCache(HWND hwndNotify, UINT uMsgNotify, WPARAM wParamNotify, LPARAM lParamNotify)
{
	if (bReadingImages || bSavingCache) {
		tg.wait();
		bReadingImages = bSavingCache = false;
	}

	tg.run([=] { Cache.Save(FirstIcon(), whIcon, hwndNotify, uMsgNotify, wParamNotify, lParamNotify); });
}
