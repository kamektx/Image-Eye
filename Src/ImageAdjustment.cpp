//-----------------------------------------------------------------------------
//   Image Eye - an Open Source image viewer
//   Copyright 2015 by Markus Dimdal and FMJ-Software.
//-----------------------------------------------------------------------------
//   CONTENTS:	Image adjustment dialog
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

#include "muiMain.h"
#include "mdDlgCtrls.h"
#include "../Res/resource.h"


//-----------------------------------------------------------------------------

inline int NiceRound(float v)
{
	return (v >= 0) ? int(v + 0.5f) : -int(-v + 0.5f);
}


static bool AnythingToReset(iePImageDisplay pimd)
// Are any adjusment at non-zero?
{
	return	pimd->Adjustments()->AnyColorAdjustment() || 
			(pimd->Adjustments()->GetSharpness() != 0.0f) || 
			(pimd->Adjustments()->GetRotation() != ie_ParseOrientationInfoStr(pimd->Text()->Get(ieTextInfoType::Orientation)));
}


static void OnAdjustmentsChanged(HWND hwndDlg, HWND hwndImage, iePImageDisplay pimd)
// Re-calculate & update the display image
{
	static volatile int nUpdateCounter = 0;
	if (nUpdateCounter++) return;	// If calculations are already underway, just flag that it needs to be redone

	// Calculate display image
	HCURSOR hcurOld = SetCursor(LoadCursor(NULL, IDC_WAIT));

	for (;;) {
		pimd->AdjustmentsHasChanged();
	
		if (!--nUpdateCounter) break;
		nUpdateCounter = 1;
	}

	SetCursor(hcurOld);

	// Update window
	InvalidateRect(hwndImage, NULL, FALSE);
	UpdateWindow(hwndImage);

	if (hwndDlg) {
		// Update adjustments dialog
		PCTCHAR pcsz = pimd->Text()->Get(ieTextInfoType::SourceFile);
		CtlItem::Enable(hwndDlg, CLR_SAVE,  pcsz && *pcsz && (*pcsz != '<'));
		CtlItem::SetFocus(hwndDlg, IDOK);
		pimd->Adjustments()->SetModifiedFlag(true);
	}
}


static void SetFInt2Ctrl(HWND hwnd, UINT idCtrl, float fValue)
// Set float to n.nn in dialog box item texts
{
	TCHAR sz[8], *p = sz;
	if (fValue < 0.0) {
		*p++ = '-';
		fValue = -fValue;
	}
	int iValue = int(fValue*100.0f + 0.5f);
	_stprintf(p, _T("%d.%d%d"), iValue/100, (iValue/10)%10, iValue%10);
	CtlItem::SetText(hwnd, idCtrl, sz);
}


static INT_PTR CALLBACK ImageAdjustmentDlg(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
// Image adjustment dialog message handler
{
	iePImageDisplay pimd = (iePImageDisplay)GetWindowLongPtr(hwnd, DWLP_USER);
	HWND hwndParent;
	RECT rcImage, rcDialog;
	muiCoord xyDisplay;
	muiSize whDisplay;
	int iL, iR, iT, iB, iW, iH, x, y;
	ieOrientation eR;
	PCTCHAR pcsz;
	TCHAR sz[128];
	bool bParentZoomed;
	bool bHaveColor;

	switch (uMsg) {

	case WM_INITDIALOG:
		ieTranslateCtrls(hwnd);
		pimd = (iePImageDisplay)lParam;
		SetWindowLongPtr(hwnd, DWLP_USER, (LONG_PTR)pimd);

		// Set small icon
		SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)(HICON)LoadImage(g_hInst, MAKEINTRESOURCE(CLRICO), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

		_stprintf(sz, _T("%s | %s"), ieTranslate(_T("Image adjustment")), _T(PROGRAMVERNAME));
		SetWindowText(hwnd, sz);

		// Set ranges and zero settings
		CtlTrck::SetRange(hwnd, CLR_CONTRAST, -100, 200, true);
		CtlTrck::SetRange(hwnd, CLR_BRIGHTNESS, -100, 200, true);
		CtlTrck::SetRange(hwnd, CLR_SHARPNESS, -100, 200, true);
		CtlTrck::SetRange(hwnd, CLR_HUE, -100, 100, true);
		CtlTrck::SetRange(hwnd, CLR_SAT, -100, 100, true);
		CtlTrck::SetRange(hwnd, CLR_GAMMA, 0, 200, true);
		CtlTrck::SetRange(hwnd, CLR_LUM, -100, 200, true);
		CtlTrck::SetRange(hwnd, CLR_R, -100, 200, true);
		CtlTrck::SetRange(hwnd, CLR_G, -100, 200, true);
		CtlTrck::SetRange(hwnd, CLR_B, -100, 200, true);
		CtlCmbo::Reset(hwnd, CLR_ROTATE);
		CtlCmbo::AddStr(hwnd, CLR_ROTATE, _T("0°"  ), int(ieOrientation::Rotate0));
		CtlCmbo::AddStr(hwnd, CLR_ROTATE, _T("-90°"), int(ieOrientation::Rotate90));
		CtlCmbo::AddStr(hwnd, CLR_ROTATE, _T("180°"), int(ieOrientation::Rotate180));
		CtlCmbo::AddStr(hwnd, CLR_ROTATE, _T("+90°"), int(ieOrientation::Rotate270));
		SendMessage(hwnd, WM_COMMAND, 999, 0);

		// Disable some controls if grayscale
		bHaveColor = !(pimd->OrigImage()->L() || pimd->OrigImage()->wL());
		CtlTrck::Enable(hwnd, CLR_HUE, bHaveColor);
		CtlTrck::Enable(hwnd, CLR_SAT, bHaveColor);
		CtlTrck::Enable(hwnd, CLR_R, bHaveColor);
		CtlTrck::Enable(hwnd, CLR_G, bHaveColor);
		CtlTrck::Enable(hwnd, CLR_B, bHaveColor);

		// Find a non-obtrusive position for the dialog...
		hwndParent = GetParent(hwnd);
		bParentZoomed = IsZoomed(hwndParent);
		GetWindowRect(hwnd, &rcDialog);
		GetWindowRect(hwndParent, &rcImage);
		muiGetWorkArea(GetParent(hwnd), xyDisplay, whDisplay, bParentZoomed);

		iW = rcDialog.right - rcDialog.left;					// Dialog width
		iH = rcDialog.bottom - rcDialog.top;					// Dialog height
		iR = xyDisplay.x + whDisplay.w - (rcImage.right + iW);	// Space available to the right of the image
		iL = rcImage.left - (xyDisplay.x + iW);					// Space available to the left of the image
		iB = xyDisplay.y + whDisplay.h - (rcImage.bottom + iH);	// Space available at the top of the image
		iT = rcImage.top - (xyDisplay.y + iH);					// Space available at the bottom of the image
		
		if (iR >= 0) {
			x = rcImage.right;
			y = rcImage.bottom - iH;
		} else if (iL >= 0) {
			x = rcImage.left - iW;
			y = rcImage.bottom - iH;
		} else {
			x = xyDisplay.x + whDisplay.w - iW;
			if (iB >= 0) {
				y = rcImage.bottom;
			} else if (iT >= 0) {
				y = rcImage.top-iH;
			} else {
				y = xyDisplay.y + whDisplay.h - iH;
			}
		}
		
		if (x+iW > xyDisplay.x + whDisplay.w) x = xyDisplay.x + whDisplay.w - iW;
		if (x < xyDisplay.x) x = xyDisplay.x;

		if (y+iH > xyDisplay.y + whDisplay.h) y = xyDisplay.y + whDisplay.h - iH;
		if (y < xyDisplay.y) y = xyDisplay.y;

		SetWindowPos(hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		
		pcsz = pimd->Text()->Get(ieTextInfoType::SourceFile);
		CtlItem::Enable(hwnd, CLR_SAVE, pimd->Adjustments()->GetModifiedFlag() && pcsz && *pcsz && (*pcsz != '<'));

		SetFocus(GetDlgItem(hwnd, IDOK));

		return FALSE;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {

		case 999:
			// Set controls
			CtlTrck::SetPos(hwnd, CLR_CONTRAST, NiceRound(pimd->Adjustments()->GetContrast()/0.005f));
			CtlTrck::SetPos(hwnd, CLR_BRIGHTNESS, NiceRound(pimd->Adjustments()->GetBrightness()/0.01f));
			CtlTrck::SetPos(hwnd, CLR_SHARPNESS, NiceRound(pimd->Adjustments()->GetSharpness()/0.01f));
			CtlTrck::SetPos(hwnd, CLR_HUE, NiceRound(pimd->Adjustments()->GetHue()/0.005f));
			CtlTrck::SetPos(hwnd, CLR_SAT, NiceRound(pimd->Adjustments()->GetSaturation()/0.01f));
			CtlTrck::SetPos(hwnd, CLR_GAMMA, NiceRound(pimd->Adjustments()->GetGamma()/0.01f));
			CtlTrck::SetPos(hwnd, CLR_LUM, NiceRound(pimd->Adjustments()->GetLuminance()/0.005f));
			CtlTrck::SetPos(hwnd, CLR_R, NiceRound(pimd->Adjustments()->GetRed()/0.005f));
			CtlTrck::SetPos(hwnd, CLR_G, NiceRound(pimd->Adjustments()->GetGreen()/0.005f));
			CtlTrck::SetPos(hwnd, CLR_B, NiceRound(pimd->Adjustments()->GetBlue()/0.005f));
			CtlButn::SetCheck(hwnd, CLR_INV, pimd->Adjustments()->GetInvert());

			eR = pimd->Adjustments()->GetRotation();
			switch (ieRotationOf(eR)) {
			case ieOrientation::Rotate0:	CtlCmbo::SetSel(hwnd, CLR_ROTATE, 0);	break;
			case ieOrientation::Rotate90:	CtlCmbo::SetSel(hwnd, CLR_ROTATE, 1);	break;
			case ieOrientation::Rotate180:	CtlCmbo::SetSel(hwnd, CLR_ROTATE, 2);	break;
			case ieOrientation::Rotate270:	CtlCmbo::SetSel(hwnd, CLR_ROTATE, 3);	break;
			}
			CtlButn::SetCheck(hwnd, CLR_MIRRORV, ieIsMirrorVert( eR));
			CtlButn::SetCheck(hwnd, CLR_MIRRORH, ieIsMirrorHoriz(eR));

			SendMessage(hwnd, WM_HSCROLL, SB_THUMBTRACK, 0);	// Set text values
			break;

		case CLR_RESET: {
			double d;
			d = pimd->Adjustments()->GetZoom();

			pimd->Adjustments()->Reset();
			pimd->Adjustments()->SetRotation(ie_ParseOrientationInfoStr(pimd->Text()->Get(ieTextInfoType::Orientation)));
			pimd->Adjustments()->SetZoom(d);
			SendMessage(hwnd, WM_COMMAND, 999, 0);

			OnAdjustmentsChanged(hwnd, GetParent(hwnd), pimd);

			SendMessage(hwnd, WM_COMMAND, CLR_SAVE | (BN_CLICKED<<16), 0);
		}	break;

		case IDCANCEL:
			EndDialog(hwnd, 0);
			break;

		case CLR_SAVE:
			pcsz = pimd->Text()->Get(ieTextInfoType::SourceFile);
			if (pcsz && *pcsz && (*pcsz != '<')) {
				if (pimd->Adjustments()->Save(pcsz, pimd->OrigImage())) {
					pimd->Adjustments()->SetModifiedFlag(false);
					CtlItem::Enable(hwnd, CLR_SAVE, false);
				}
			}
			break;

		case IDOK:
			EndDialog(hwnd, 1);
			break;

		case CLR_INV:
			if (HIWORD(wParam) != BN_CLICKED) break;
			
			pimd->Adjustments()->SetInvert(CtlButn::GetCheck(hwnd, CLR_INV));

			CtlButn::Enable(hwnd, CLR_RESET, AnythingToReset(pimd));
			
			OnAdjustmentsChanged(hwnd, GetParent(hwnd), pimd);
			break;

		case CLR_MIRRORV:
		case CLR_MIRRORH:
			if (HIWORD(wParam) != BN_CLICKED) break;
			
			eR = ieOrientation(CtlCmbo::GetSelData(hwnd, CLR_ROTATE));
			if (CtlButn::GetCheck(hwnd, CLR_MIRRORV)) eR = ieMirrorVert( eR);
			if (CtlButn::GetCheck(hwnd, CLR_MIRRORH)) eR = ieMirrorHoriz(eR);
			pimd->Adjustments()->SetRotation(eR);

			CtlButn::Enable(hwnd, CLR_RESET, AnythingToReset(pimd));
			
			OnAdjustmentsChanged(hwnd, GetParent(hwnd), pimd);
			break;

		case CLR_ROTATE:
			if (HIWORD(wParam) != CBN_SELCHANGE) break;

			eR = ieOrientation(CtlCmbo::GetSelData(hwnd, CLR_ROTATE));
			if (CtlButn::GetCheck(hwnd, CLR_MIRRORV)) eR = ieMirrorVert(eR);
			if (CtlButn::GetCheck(hwnd, CLR_MIRRORH)) eR = ieMirrorHoriz(eR);
			pimd->Adjustments()->SetRotation(eR);

			CtlButn::Enable(hwnd, CLR_RESET, AnythingToReset(pimd));
			
			OnAdjustmentsChanged(hwnd, GetParent(hwnd), pimd);
			SendMessage(GetParent(hwnd), WM_COMMAND, CMD_ONNEWIMAGE, 0);
			break;

		}
		return 0;

	case WM_CLOSE:
		PostMessage(hwnd, WM_COMMAND, IDCANCEL, 0);
		return 0;

	case WM_HSCROLL:
		pimd->Adjustments()->SetContrast(0.005f*CtlTrck::GetPos(hwnd, CLR_CONTRAST));
		SetFInt2Ctrl(hwnd, CLR_CONTRAST_VAL, 2.0f * pimd->Adjustments()->GetContrast());
		
		pimd->Adjustments()->SetBrightness(0.01f*CtlTrck::GetPos(hwnd, CLR_BRIGHTNESS));
		SetFInt2Ctrl(hwnd, CLR_BRIGHTNESS_VAL, pimd->Adjustments()->GetBrightness());

		pimd->Adjustments()->SetSharpness(0.01f*CtlTrck::GetPos(hwnd, CLR_SHARPNESS));
		SetFInt2Ctrl(hwnd, CLR_SHARPNESS_VAL, pimd->Adjustments()->GetSharpness());

		pimd->Adjustments()->SetHue(0.005f*CtlTrck::GetPos(hwnd, CLR_HUE));
		SetFInt2Ctrl(hwnd, CLR_HUE_VAL, 2.0f * pimd->Adjustments()->GetHue());

		pimd->Adjustments()->SetSaturation(0.01f*CtlTrck::GetPos(hwnd, CLR_SAT));
		SetFInt2Ctrl(hwnd, CLR_SAT_VAL, pimd->Adjustments()->GetSaturation());

		pimd->Adjustments()->SetGamma(0.01f*CtlTrck::GetPos(hwnd, CLR_GAMMA));
		SetFInt2Ctrl(hwnd, CLR_GAMMA_VAL, pimd->Adjustments()->GetGamma());

		pimd->Adjustments()->SetLuminance(0.005f*CtlTrck::GetPos(hwnd, CLR_LUM));
		SetFInt2Ctrl(hwnd, CLR_LUM_VAL, 2.0f * pimd->Adjustments()->GetLuminance());

		pimd->Adjustments()->SetRed(0.005f*CtlTrck::GetPos(hwnd, CLR_R));
		SetFInt2Ctrl(hwnd, CLR_R_VAL, 2.0f * pimd->Adjustments()->GetRed());

		pimd->Adjustments()->SetGreen(0.005f*CtlTrck::GetPos(hwnd, CLR_G));
		SetFInt2Ctrl(hwnd, CLR_G_VAL, 2.0f * pimd->Adjustments()->GetGreen());

		pimd->Adjustments()->SetBlue(0.005f*CtlTrck::GetPos(hwnd, CLR_B));
		SetFInt2Ctrl(hwnd, CLR_B_VAL, 2.0f * pimd->Adjustments()->GetBlue());

		CtlButn::Enable(hwnd, CLR_RESET, AnythingToReset(pimd));

		if (LOWORD(wParam) == SB_ENDSCROLL) {
			OnAdjustmentsChanged(hwnd, GetParent(hwnd), pimd);
		}

		return 0;
	}

	return FALSE;
}


void ShowImageAdjustmentDialog(HWND hwnd, iePImageDisplay &pimd)
// Show image adjustment dialog
{
	ieImageAdjustParams iapOld = *pimd->Adjustments();

	bool bOk = DialogBoxParam(g_hInst, MAKEINTRESOURCE(CLRDLG), hwnd, &ImageAdjustmentDlg, (LPARAM)pimd);

	if (!bOk) {
		*pimd->Adjustments() = iapOld;
		OnAdjustmentsChanged(NULL, hwnd, pimd);
	}
}
