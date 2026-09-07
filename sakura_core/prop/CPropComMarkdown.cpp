/*!	@file
	@brief 共通設定ダイアログボックス、「Markdown」ページ（【自前改造】）

	`.md` を開いたときの見た目（見出しの大きさ・余白・字間・書体）をここで変える。
	🔥 **数値の意味と限界は `util/markdown.h` が正本。** ここは入れ物と入力の見張りだけ。

	⚠ **行の高さは設定項目にしていない。** 見出し1の大きさから自動で決まる（markdown.h 参照）。
	   ここを人が触れるようにすると「見出しを大きくしたのに変わらない」を自分で作れてしまう。
*/
/*
	Copyright (C) 2026, Sakura Editor Organization

	This source code is designed for sakura editor.
	Please contact the copyright holders to use this code for other purpose.
*/

#include "StdAfx.h"
#include "prop/CPropCommon.h"
#include "util/markdown.h"
#include "util/shell.h"
#include "sakura_rc.h"
#include "sakura.hh"

static const DWORD p_helpids[] = {
	0, 0
};

namespace {

//! 入力された数字を、安全な範囲に丸めて取り出す
int GetNum( HWND hwndDlg, int nId, int nMin, int nMax, int nDefault )
{
	BOOL bOk = FALSE;
	int n = (int)::GetDlgItemInt( hwndDlg, nId, &bOk, FALSE );
	if( !bOk ){
		return nDefault;	// 空欄・数字でない → 既定値に戻す（黙って壊れた値を入れない）
	}
	if( n < nMin ){ n = nMin; }
	if( nMax < n ){ n = nMax; }
	return n;
}

} // namespace

/*!
	@param hwndDlg ダイアログボックスのWindow Handle
	@param uMsg メッセージ
	@param wParam パラメータ1
	@param lParam パラメータ2
*/
INT_PTR CALLBACK CPropMarkdown::DlgProc_page(
	HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	return DlgProc( reinterpret_cast<pDispatchPage>(&CPropMarkdown::DispatchEvent), hwndDlg, uMsg, wParam, lParam );
}

/* メッセージ処理 */
INT_PTR CPropMarkdown::DispatchEvent(
	HWND	hwndDlg,
	UINT	uMsg,
	WPARAM	wParam,
	LPARAM	lParam
)
{
	NMHDR*	pNMHDR;

	switch( uMsg ){

	case WM_INITDIALOG:
		SetData( hwndDlg );
		::SetWindowLongPtr( hwndDlg, DWLP_USER, lParam );
		return TRUE;

	case WM_COMMAND:
		if( IDC_BUTTON_MD_DEFAULT == LOWORD( wParam ) && BN_CLICKED == HIWORD( wParam ) ){
			// 🔥 既定値は util/markdown.h の MD_DEF_*。ここに数字を直接書かない（二重管理になる）
			::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_H1,      MD_DEF_HEADING_SCALE[0], FALSE );
			::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_H2,      MD_DEF_HEADING_SCALE[1], FALSE );
			::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_H3,      MD_DEF_HEADING_SCALE[2], FALSE );
			::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_GAP,     MD_DEF_HEADING_GAP,      FALSE );
			::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_NARROW,  MD_DEF_HEADING_NARROW,   FALSE );
			::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_SPACING, MD_DEF_CHAR_SPACING,     FALSE );
			::SetDlgItemText( hwndDlg, IDC_EDIT_MD_FACE,   MD_DEF_HEADING_FACE );
			::CheckDlgButton( hwndDlg, IDC_CHECK_MD_BOLD, BST_CHECKED );
			return TRUE;
		}
		break;

	case WM_NOTIFY:
		pNMHDR = (NMHDR*)lParam;
		switch( pNMHDR->code ){
		case PSN_HELP:
			OnHelp( hwndDlg, IDD_PROP_MARKDOWN );
			return TRUE;
		case PSN_KILLACTIVE:
			GetData( hwndDlg );
			return TRUE;
		case PSN_SETACTIVE:
			m_nPageNum = ID_PROPCOM_PAGENUM_MARKDOWN;
			return TRUE;
		default:
			break;
		}
		break;

	case WM_HELP:
		{
			HELPINFO* p = (HELPINFO*)lParam;
			MyWinHelp( (HWND)p->hItemHandle, HELP_WM_HELP, (ULONG_PTR)(LPVOID)p_helpids );
		}
		return TRUE;

	case WM_CONTEXTMENU:
		MyWinHelp( hwndDlg, HELP_CONTEXTMENU, (ULONG_PTR)(LPVOID)p_helpids );
		return TRUE;

	default:
		break;
	}
	return FALSE;
}

/* ダイアログデータの設定 */
void CPropMarkdown::SetData( HWND hwndDlg )
{
	const CommonSetting_Markdown& md = m_Common.m_sMarkdown;

	::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_H1,      md.m_nHeadingScale[0], FALSE );
	::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_H2,      md.m_nHeadingScale[1], FALSE );
	::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_H3,      md.m_nHeadingScale[2], FALSE );
	::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_GAP,     md.m_nHeadingGap,      FALSE );
	::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_NARROW,  md.m_nHeadingNarrow,   FALSE );
	::SetDlgItemInt( hwndDlg, IDC_EDIT_MD_SPACING, md.m_nCharSpacing,     FALSE );
	::SetDlgItemText( hwndDlg, IDC_EDIT_MD_FACE,   md.m_szHeadingFace );
	::CheckDlgButton( hwndDlg, IDC_CHECK_MD_BOLD, md.m_bHeadingBold ? BST_CHECKED : BST_UNCHECKED );

	// 書体名は LF_FACESIZE を超えて入れさせない（超えると黙って切れる）
	::SendDlgItemMessage( hwndDlg, IDC_EDIT_MD_FACE, EM_LIMITTEXT, (WPARAM)(LF_FACESIZE - 1), 0 );
}

/* ダイアログデータの取得 */
int CPropMarkdown::GetData( HWND hwndDlg )
{
	CommonSetting_Markdown& md = m_Common.m_sMarkdown;

	// 🔥 範囲の外は必ず丸める。見出しが行より高いと頭打ちになるだけだが、
	//    0 や負を入れられると字が消える・落ちるので、ここで止める。
	md.m_nHeadingScale[0] = GetNum( hwndDlg, IDC_EDIT_MD_H1,      100, 300, MD_DEF_HEADING_SCALE[0] );
	md.m_nHeadingScale[1] = GetNum( hwndDlg, IDC_EDIT_MD_H2,      100, 300, MD_DEF_HEADING_SCALE[1] );
	md.m_nHeadingScale[2] = GetNum( hwndDlg, IDC_EDIT_MD_H3,      100, 300, MD_DEF_HEADING_SCALE[2] );
	md.m_nHeadingGap      = GetNum( hwndDlg, IDC_EDIT_MD_GAP,       0, 100, MD_DEF_HEADING_GAP );
	md.m_nHeadingNarrow   = GetNum( hwndDlg, IDC_EDIT_MD_NARROW,    0,   3, MD_DEF_HEADING_NARROW );
	md.m_nCharSpacing     = GetNum( hwndDlg, IDC_EDIT_MD_SPACING,   0,   8, MD_DEF_CHAR_SPACING );
	md.m_bHeadingBold     = ( BST_CHECKED == ::IsDlgButtonChecked( hwndDlg, IDC_CHECK_MD_BOLD ) );

	::GetDlgItemText( hwndDlg, IDC_EDIT_MD_FACE, md.m_szHeadingFace, LF_FACESIZE );
	md.m_szHeadingFace[LF_FACESIZE - 1] = L'\0';

	return TRUE;
}
