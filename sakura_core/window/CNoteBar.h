/*! @file
	@brief 【自前改造】ノートバー（左サイドバー）

	退避フォルダーに溜まったメモを新しい順に並べ、クリックで開くための
	左サイドバー。編集ウィンドウ（CEditWnd）の子ウィンドウとして生きる。
	詳細は MY_MODS.md 改造⑪ を参照。

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_CNOTEBAR_H_
#define SAKURA_CNOTEBAR_H_
#pragma once

#include "CWnd.h"

#include <string>
#include <vector>

//! ノートバー（左サイドバー）
class CNoteBar final : public CWnd
{
public:
	CNoteBar();
	~CNoteBar() override;

	HWND Open( HINSTANCE hInstance, HWND hwndParent );	//!< ウィンドウを作る
	void Close();										//!< ウィンドウを壊す

	void Refresh( bool bForce = false );				//!< 一覧を作り直す（中身が同じなら何もしない）
	void UpdateTheme();									//!< ダークモード切替時のフォント・色の作り直し
	void ApplyCollapsed();								//!< 開閉が切り替わったときの作り直し

	int  GetBarWidth() const;							//!< 今の幅（ピクセル・DPI補正済み）

	static bool IsCollapsed();							//!< 畳んである（細い帯だけ）か
	static int  GetDefaultWidth();						//!< 既定の幅（DPI補正前）
	static std::wstring GetNoteFolder();				//!< 一覧に出すフォルダー

protected:
	/* 仮想関数 メッセージ処理 */
	LRESULT DispatchEvent( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT DispatchEvent_WM_APP( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT OnSize( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT OnPaint( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT OnCommand( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT OnDrawItem( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT OnMeasureItem( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT OnLButtonDown( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT OnLButtonUp( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT OnMouseMove( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;
	LRESULT OnDestroy( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) override;

private:
	//! 一覧の1件
	struct SNote {
		std::wstring	strPath;	//!< フルパス
		std::wstring	strTitle;	//!< 見出し（日時と拡張子を落としたもの）
		std::wstring	strDate;	//!< 日時（小さく灰色で出す）
	};

	void CreateFonts();
	void DestroyFonts();
	void LayoutChildren();
	void NotifyParentLayout();				//!< 親に配置し直してもらう
	void OpenNote( int nIndex );			//!< 開く
	void ShowNoteMenu( int nIndex, POINT ptScreen );	//!< 右クリックメニュー
	void RenameNote( int nIndex );
	void DeleteNote( int nIndex );
	void RevealNote( int nIndex );
	int  HitTestList( POINT ptClient ) const;
	bool IsInGrip( POINT ptClient ) const;
	bool IsInHeader( POINT ptClient ) const;
	bool IsInCloseBtn( POINT ptClient ) const;	//!< 開閉ボタンの上か（畳んでいるときは帯全体）
	void GetHeaderRects( RECT* pRcHeader, RECT* pRcClose ) const;
	void DrawChevron( HDC hdc, const RECT& rcBtn, COLORREF clrLine, bool bToRight ) const;	//!< ◀ ▶ を線で描く
	void SelectCurrentDocument();			//!< 今開いている文書を選択状態にする

	static void SplitNoteName( LPCWSTR pszPath, std::wstring* pStrTitle, std::wstring* pStrDate );

	std::vector<SNote>	m_vNotes;
	HWND				m_hwndList		= nullptr;
	HFONT				m_hFontTitle	= nullptr;
	HFONT				m_hFontSub		= nullptr;
	HBRUSH				m_hbrBack		= nullptr;
	int					m_nItemHeight	= 0;
	bool				m_bHeaderHot	= false;
	bool				m_bHeaderDown	= false;
	bool				m_bCloseHot		= false;
	bool				m_bCloseDown	= false;
	bool				m_bSizing		= false;
	int					m_nSizeOrgX		= 0;
	int					m_nSizeOrgWidth	= 0;
	bool				m_bTracking		= false;
	ULONGLONG			m_ullLastScan	= 0;	//!< 最後にフォルダーを見た時刻
};

#endif /* SAKURA_CNOTEBAR_H_ */
