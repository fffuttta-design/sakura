/*! @file
	@brief 【自前改造】ノートバー（左サイドバー）

	退避フォルダーに溜まったメモを新しい順に並べ、クリックで開くための
	左サイドバー。編集ウィンドウ（CEditWnd）の子ウィンドウとして生きる。
	ドラッグで自分好みに並べ替えられ、その順はフォルダー内の覚え書きに残る。
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
	static bool IsFolderWaiting();					//!< 一覧に出すフォルダーがまだ現れていない（ドライブの接続待ち）か

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
	void ShowStatus( LPCWSTR pszText, LPCWSTR pszSub );	//!< 一覧の代わりに一言だけ出す（接続待ちなど）
	int  HitTestList( POINT ptClient ) const;

	/* 並べ替え（ドラッグ＆ドロップ） */
	void LoadOrder();									//!< 覚え書き（並び順ファイル）を読む
	void SaveOrder() const;								//!< 今の並びを覚え書きへ書く
	void ResetOrder();									//!< 覚え書きを捨てて「新しい順」に戻す
	void ApplyOrder( std::vector<std::wstring>& vFiles ) const;	//!< 覚えた順に並べ替える
	void MoveNote( int nFrom, int nInsertAt );			//!< ドラッグの結果を反映する
	int  HitTestInsert( POINT ptList ) const;			//!< どこへ差し込むか（0〜件数）
	void SetDropAt( int nDropAt );						//!< 差し込み位置が変わったら描き直す
	void EndDrag( bool bApply );						//!< 掴んでいる状態を終える
	void DrawInsertMark( HDC hdc ) const;				//!< 差し込み位置の線を引く
	void AutoScrollForDrag();							//!< 端まで持っていったら一覧を送る

	static std::wstring GetOrderFilePath();				//!< 並び順の覚え書きの置き場所
	static LRESULT CALLBACK ListProc( HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR );	//!< 一覧の横取り
	bool IsInGrip( POINT ptClient ) const;
	bool IsInCloseBtn( POINT ptClient ) const;	//!< 開閉ボタンの上か（上の帯ぜんぶ・畳んでいるときは細い帯ぜんぶ）
	void GetHeaderRect( RECT* pRcHeader ) const;
	void DrawChevron( HDC hdc, const RECT& rcBtn, COLORREF clrLine, bool bToRight, int nArmRaw ) const;	//!< ◀ ▶ を線で描く
	void SelectCurrentDocument();			//!< 今開いている文書を選択状態にする

	static void SplitNoteName( LPCWSTR pszPath, std::wstring* pStrTitle, std::wstring* pStrDate );

	std::vector<SNote>	m_vNotes;
	std::vector<std::wstring>	m_vOrder;	//!< 覚えている並び順（ファイル名だけ・上から順）
	std::wstring		m_strStatus;		//!< 一覧の代わりに出している一言（空なら普通の一覧）
	std::wstring		m_strStatusSub;		//!< その下に小さく出す説明
	HWND				m_hwndList		= nullptr;
	HFONT				m_hFontTitle	= nullptr;
	HFONT				m_hFontSub		= nullptr;
	HBRUSH				m_hbrBack		= nullptr;
	int					m_nItemHeight	= 0;
	bool				m_bCloseHot		= false;
	bool				m_bCloseDown	= false;
	bool				m_bSizing		= false;
	int					m_nSizeOrgX		= 0;
	int					m_nSizeOrgWidth	= 0;
	bool				m_bTracking		= false;
	bool				m_bMayDrag		= false;	//!< 一覧を押した（まだ動かしていない）
	bool				m_bDragging		= false;	//!< 掴んで動かしている
	int					m_nDragFrom		= -1;		//!< 掴んだ位置
	int					m_nDropAt		= -1;		//!< 差し込む位置（0〜件数）
	POINT				m_ptDragStart	= { 0, 0 };	//!< 押した所（動かしたと見なす距離の起点）
	bool				m_bDragScroll	= false;	//!< 端まで持っていったときの自動送り（タイマー）
	bool				m_bWaitTimer	= false;	//!< 接続待ちの見張り（タイマー）を動かしているか
	ULONGLONG			m_ullLastScan	= 0;	//!< 最後にフォルダーを見た時刻
};

#endif /* SAKURA_CNOTEBAR_H_ */
