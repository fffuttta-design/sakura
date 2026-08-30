/*! @file
	@brief 【自前改造】ノートバー（左サイドバー）

	SPDX-License-Identifier: Zlib
*/
#include "StdAfx.h"
#include "CNoteBar.h"

#include "window/CEditWnd.h"
#include "doc/CEditDoc.h"
#include "doc/CDocListener.h"
#include "dlg/CDlgInput1.h"
#include "env/CShareData.h"
#include "env/CAppNodeManager.h"
#include "util/file.h"
#include "util/quickstash.h"
#include "util/window.h"
#include "util/MessageBoxF.h"
#include "uiparts/CGraphics.h"
#include "apiwrap/StdApi.h"
#include "apiwrap/DarkMode.h"
#include "_main/global.h"
#include "config/system_constants.h"
#include "_main/CControlTray.h"
#include "Funccode_enum.h"

#include <algorithm>
#include <cstdlib>
#include <shellapi.h>

namespace {

//! 見た目の寸法（DPI補正前）
constexpr int NOTEBAR_HEADER_H	= 30;	//!< 上の帯（畳むボタン）の高さ
constexpr int NOTEBAR_GRIP_W	= 4;	//!< 右端の掴んで幅を変える所
constexpr int NOTEBAR_PAD		= 6;	//!< 内側の余白
constexpr int NOTEBAR_MIN_W		= 120;	//!< これより細くしない
constexpr int NOTEBAR_MAX_W		= 600;	//!< これより太くしない
constexpr int NOTEBAR_MAX_ITEMS	= 300;	//!< 一覧に出す上限
constexpr UINT_PTR IDT_NOTEBAR_WAIT = 1;	//!< ドライブの接続待ちを見張るタイマー
constexpr UINT_PTR IDT_NOTEBAR_DRAG = 2;	//!< 掴んだまま端で止めたときに一覧を送るタイマー
constexpr UINT   NOTEBAR_DRAG_SCROLL_MS = 90;	//!< その送る間隔
constexpr UINT_PTR NOTEBAR_LIST_SUBCLASS = 1;	//!< 一覧を横取りするときの目印
constexpr ULONG_PTR NOTEBAR_STATUS_ITEM = (ULONG_PTR)-1;	//!< 一覧の代わりに出す一言の目印
constexpr int NOTEBAR_FOLD_W	= 16;	//!< 畳んだときに残す帯の幅（帯ぜんぶが「開く」ボタン）

//! 一覧の子ウィンドウID
constexpr int IDC_NOTEBAR_LIST	= 1001;

//! 自分宛の内部メッセージ
constexpr UINT WM_NOTEBAR_OPEN	= WM_APP + 21;	//!< wParam = 一覧の位置

//! 右クリックメニューのID
enum ENoteMenu {
	NOTEMENU_OPEN = 1,
	NOTEMENU_RENAME,
	NOTEMENU_DELETE,
	NOTEMENU_REVEAL,
	NOTEMENU_FOLDER,
	NOTEMENU_RESETORDER,
};

} // namespace

CNoteBar::CNoteBar()
	: CWnd( L"CNoteBar" )
{
}

CNoteBar::~CNoteBar()
{
	DestroyFonts();
}

/*! 既定の幅（DPI補正前） */
int CNoteBar::GetDefaultWidth()
{
	return 210;
}

/*! 一覧に出すフォルダー

	共通設定で指定されていればそちら、空なら退避フォルダー。
*/
std::wstring CNoteBar::GetNoteFolder()
{
	const WCHAR* pszFolder = GetDllShareData().m_Common.m_sWindow.m_szNoteBarFolder;
	if( pszFolder[0] != L'\0' && IsDirectory( pszFolder ) ){
		return std::wstring( pszFolder );
	}
	return GetQuickStashDir();
}

/*! 畳んである（細い帯だけ出ている）か

	🔥 閉じても窓ごと消さず、幅 NOTEBAR_FOLD_W の帯だけ残す。
	   そうしないと「閉じたあと開き直すボタン」が画面のどこにも無くなる。
*/
/*! 一覧に出すフォルダーがまだ現れていない（ドライブの接続待ち）か

	設定で決め打ちしたフォルダーが在るなら、それが正＝待ちではない。
*/
bool CNoteBar::IsFolderWaiting()
{
	const WCHAR* pszFolder = GetDllShareData().m_Common.m_sWindow.m_szNoteBarFolder;
	if( pszFolder[0] != L'\0' && IsDirectory( pszFolder ) ){
		return false;
	}
	return IsQuickStashDriveWaiting();
}

bool CNoteBar::IsCollapsed()
{
	return ( 0 == GetDllShareData().m_Common.m_sWindow.m_bDispNoteBar );
}

/*! 今の幅（ピクセル・DPI補正済み） */
int CNoteBar::GetBarWidth() const
{
	if( IsCollapsed() ){
		return ::DpiScaleX( NOTEBAR_FOLD_W );
	}
	int nWidth = GetDllShareData().m_Common.m_sWindow.m_nNoteBarWidth;
	if( nWidth < NOTEBAR_MIN_W ) nWidth = NOTEBAR_MIN_W;
	if( nWidth > NOTEBAR_MAX_W ) nWidth = NOTEBAR_MAX_W;
	return ::DpiScaleX( nWidth );
}

HWND CNoteBar::Open( HINSTANCE hInstance, HWND hwndParent )
{
	if( GetHwnd() ){
		return GetHwnd();
	}

	LPCWSTR pszClassName = L"CNoteBar";
	RegisterWC(
		hInstance,
		nullptr,
		nullptr,
		::LoadCursor( nullptr, IDC_ARROW ),
		nullptr,				// 背景は WM_PAINT で描く（ちらつき防止）
		nullptr,
		pszClassName
	);

	CWnd::Create(
		hwndParent,
		0,
		pszClassName,
		pszClassName,
		WS_CHILD | WS_CLIPCHILDREN,
		0, 0, GetBarWidth(), 100,
		nullptr
	);
	if( !GetHwnd() ){
		return nullptr;
	}

	CreateFonts();

	m_hwndList = ::CreateWindowEx(
		0,
		L"LISTBOX",
		L"",
		WS_CHILD | WS_VISIBLE | WS_VSCROLL
			| LBS_OWNERDRAWFIXED | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT | LBS_HASSTRINGS,
		0, 0, 10, 10,
		GetHwnd(),
		(HMENU)(INT_PTR)IDC_NOTEBAR_LIST,
		GetAppInstance(),
		nullptr
	);
	if( m_hwndList ){
		::SendMessage( m_hwndList, LB_SETITEMHEIGHT, 0, (LPARAM)m_nItemHeight );
		// 🔥 ドラッグで並べ替えるために左ボタンを横取りする。
		//    ダークモードの着せ替えも同じ一覧を subclass するが、鎖でつながるので共存できる
		//    （こちらを先に掛けて、通す物は DefSubclassProc でそちらへ渡す）。
		::SetWindowSubclass( m_hwndList, &ListProc, NOTEBAR_LIST_SUBCLASS, (DWORD_PTR)this );
		if( IsDarkModeActive() ){
			DarkMode::setChildCtrlsSubclassAndTheme( GetHwnd() );
		}
	}

	Refresh( true );
	LayoutChildren();
	return GetHwnd();
}

void CNoteBar::Close()
{
	if( GetHwnd() ){
		::DestroyWindow( GetHwnd() );
		_SetHwnd( nullptr );
	}
	m_hwndList = nullptr;
	m_vNotes.clear();
	m_vOrder.clear();
	DestroyFonts();
}

void CNoteBar::CreateFonts()
{
	DestroyFonts();

	NONCLIENTMETRICS ncm;
	ncm.cbSize = sizeof(ncm);
	LOGFONT lf;
	if( ::SystemParametersInfo( SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0 ) ){
		lf = ncm.lfMessageFont;
	}else{
		::ZeroMemory( &lf, sizeof(lf) );
		lf.lfHeight = -12;
		wcscpy_s( lf.lfFaceName, L"Meiryo UI" );
	}

	LOGFONT lfTitle = lf;
	m_hFontTitle = ::CreateFontIndirect( &lfTitle );

	LOGFONT lfSub = lf;
	lfSub.lfHeight = (lf.lfHeight * 85) / 100;	// 日時は一回り小さく
	m_hFontSub = ::CreateFontIndirect( &lfSub );

	// 1件の高さ＝見出し＋日時＋余白
	int nTitleH = 16;
	int nSubH   = 13;
	const HDC hdc = ::GetDC( GetHwnd() );
	if( hdc ){
		TEXTMETRIC tm;
		HFONT hOld = (HFONT)::SelectObject( hdc, m_hFontTitle );
		if( ::GetTextMetrics( hdc, &tm ) ) nTitleH = tm.tmHeight;
		::SelectObject( hdc, m_hFontSub );
		if( ::GetTextMetrics( hdc, &tm ) ) nSubH = tm.tmHeight;
		::SelectObject( hdc, hOld );
		::ReleaseDC( GetHwnd(), hdc );
	}
	m_nItemHeight = nTitleH + nSubH + ::DpiScaleY( 10 );

	const COLORREF clrBack = IsDarkModeActive()
		? DarkMode::getDlgBackgroundColor() : ::GetSysColor( COLOR_WINDOW );
	m_hbrBack = ::CreateSolidBrush( clrBack );
}

void CNoteBar::DestroyFonts()
{
	if( m_hFontTitle ){ ::DeleteObject( m_hFontTitle ); m_hFontTitle = nullptr; }
	if( m_hFontSub   ){ ::DeleteObject( m_hFontSub   ); m_hFontSub   = nullptr; }
	if( m_hbrBack    ){ ::DeleteObject( m_hbrBack    ); m_hbrBack    = nullptr; }
}

/*! 開閉が切り替わったときの作り直し

	開いたときだけ一覧を読み直す（畳んでいる間はフォルダーを見に行かない）。
*/
void CNoteBar::ApplyCollapsed()
{
	if( !GetHwnd() ){
		return;
	}
	m_bCloseHot = false;
	if( !IsCollapsed() ){
		Refresh( true );
	}
	LayoutChildren();
	::InvalidateRect( GetHwnd(), nullptr, TRUE );
}

void CNoteBar::UpdateTheme()
{
	if( !GetHwnd() ){
		return;
	}
	CreateFonts();
	if( m_hwndList ){
		::SendMessage( m_hwndList, LB_SETITEMHEIGHT, 0, (LPARAM)m_nItemHeight );
	}
	::InvalidateRect( GetHwnd(), nullptr, TRUE );
}

/*! ファイル名を「見出し」と「日時」に割る

	クイック退避が付ける `2026-08-25_1218 見出し.txt` の日時部分を畳んで、
	見出しだけを大きく見せる。日時が付いていないファイルは更新日時を使う。
*/
void CNoteBar::SplitNoteName( LPCWSTR pszPath, std::wstring* pStrTitle, std::wstring* pStrDate )
{
	WCHAR szName[_MAX_FNAME];
	WCHAR szExt [_MAX_EXT];
	my_splitpath_t( pszPath, nullptr, nullptr, szName, szExt );

	std::wstring strName = szName;
	std::wstring strDate;

	// 「YYYY-MM-DD_hhmm 」で始まっていれば、そこを日時として切り出す
	auto isDigits = []( const std::wstring& s, size_t nPos, size_t nLen ){
		if( s.length() < nPos + nLen ) return false;
		for( size_t i = 0; i < nLen; ++i ){
			if( s[nPos + i] < L'0' || L'9' < s[nPos + i] ) return false;
		}
		return true;
	};
	if( 15 <= strName.length()
	 && isDigits( strName, 0, 4 ) && L'-' == strName[4]
	 && isDigits( strName, 5, 2 ) && L'-' == strName[7]
	 && isDigits( strName, 8, 2 ) && L'_' == strName[10]
	 && isDigits( strName, 11, 4 ) ){
		strDate  = strName.substr( 0, 4 );	// YYYY
		strDate += L"/";
		strDate += strName.substr( 5, 2 );
		strDate += L"/";
		strDate += strName.substr( 8, 2 );
		strDate += L" ";
		strDate += strName.substr( 11, 2 );
		strDate += L":";
		strDate += strName.substr( 13, 2 );
		strName  = strName.substr( 15 );
		while( !strName.empty() && (L' ' == strName.front() || L'_' == strName.front()) ){
			strName.erase( strName.begin() );
		}
	}else{
		// 日時が名前に入っていないファイルは、更新日時を出す
		WIN32_FILE_ATTRIBUTE_DATA fad;
		if( ::GetFileAttributesEx( pszPath, GetFileExInfoStandard, &fad ) ){
			SYSTEMTIME stUtc, stLocal;
			if( ::FileTimeToSystemTime( &fad.ftLastWriteTime, &stUtc )
			 && ::SystemTimeToTzSpecificLocalTime( nullptr, &stUtc, &stLocal ) ){
				WCHAR szBuf[64];
				::wsprintf( szBuf, L"%d/%02d/%02d %02d:%02d",
					stLocal.wYear, stLocal.wMonth, stLocal.wDay, stLocal.wHour, stLocal.wMinute );
				strDate = szBuf;
			}
		}
	}

	if( strName.empty() ){
		strName = szName;	// 日時だけの名前だったとき
	}
	// txt 以外は拡張子も見せる（何のファイルか分かるように）
	if( 0 != _wcsicmp( szExt, L".txt" ) ){
		strName += szExt;
	}

	if( pStrTitle ) *pStrTitle = strName;
	if( pStrDate  ) *pStrDate  = strDate;
}

namespace {

//! フルパスからファイル名だけ取り出す
std::wstring NoteFileName( const std::wstring& strPath )
{
	const size_t nPos = strPath.find_last_of( L"\\/" );
	return ( std::wstring::npos == nPos ) ? strPath : strPath.substr( nPos + 1 );
}

} // namespace

/*! 並び順の覚え書きの置き場所

	一覧に出しているフォルダーの中に置く。∴ Google ドライブ越しに2台で同じ並びになる。
*/
std::wstring CNoteBar::GetOrderFilePath()
{
	const std::wstring strDir = GetNoteFolder();
	if( strDir.empty() ){
		return std::wstring();
	}
	std::wstring strPath = strDir;
	if( L'\\' != strPath.back() ){
		strPath += L'\\';
	}
	strPath += NOTEBAR_ORDER_FILE_NAME;
	return strPath;
}

/*! 覚え書き（並び順ファイル）を読む

	1行1ファイル名。`#` で始まる行と空行は読み飛ばす。
	書くときは UTF-16LE だが、手で直されても困らないよう UTF-8 も読める。
*/
void CNoteBar::LoadOrder()
{
	m_vOrder.clear();

	const std::wstring strPath = GetOrderFilePath();
	if( strPath.empty() ){
		return;
	}
	const HANDLE hFile = ::CreateFile( strPath.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	if( INVALID_HANDLE_VALUE == hFile ){
		return;		// まだ並べ替えたことがない＝新しい順のまま
	}
	LARGE_INTEGER liSize;
	std::vector<BYTE> vRaw;
	if( ::GetFileSizeEx( hFile, &liSize ) && 0 < liSize.QuadPart && liSize.QuadPart < 1024 * 1024 ){
		vRaw.resize( (size_t)liSize.QuadPart );
		DWORD dwRead = 0;
		if( !::ReadFile( hFile, &vRaw[0], (DWORD)vRaw.size(), &dwRead, nullptr ) ){
			vRaw.clear();
		}else{
			vRaw.resize( dwRead );
		}
	}
	::CloseHandle( hFile );
	if( vRaw.empty() ){
		return;
	}

	std::wstring strAll;
	if( 2 <= vRaw.size() && 0xFF == vRaw[0] && 0xFE == vRaw[1] ){
		strAll.assign( (const WCHAR*)(&vRaw[0] + 2), (vRaw.size() - 2) / sizeof(WCHAR) );
	}else{
		size_t nSkip = ( 3 <= vRaw.size() && 0xEF == vRaw[0] && 0xBB == vRaw[1] && 0xBF == vRaw[2] ) ? 3 : 0;
		const int nLen = ::MultiByteToWideChar( CP_UTF8, 0, (LPCSTR)(&vRaw[0] + nSkip),
			(int)(vRaw.size() - nSkip), nullptr, 0 );
		if( 0 < nLen ){
			strAll.resize( (size_t)nLen );
			::MultiByteToWideChar( CP_UTF8, 0, (LPCSTR)(&vRaw[0] + nSkip),
				(int)(vRaw.size() - nSkip), &strAll[0], nLen );
		}
	}

	size_t nBegin = 0;
	while( nBegin <= strAll.size() ){
		size_t nEnd = strAll.find_first_of( L"\r\n", nBegin );
		if( std::wstring::npos == nEnd ) nEnd = strAll.size();
		std::wstring strLine = strAll.substr( nBegin, nEnd - nBegin );
		nBegin = nEnd + 1;
		while( !strLine.empty() && (L' ' == strLine.back() || L'\t' == strLine.back()) ){
			strLine.pop_back();
		}
		if( !strLine.empty() && L'#' != strLine[0] ){
			m_vOrder.push_back( strLine );
		}
		if( nEnd >= strAll.size() ){
			break;
		}
	}
}

/*! 今の並びを覚え書きへ書く

	画面に出ている順をそのまま書く＝1回でも並べ替えたら、その時点の全件が固定される。
	以後に増えたメモだけが「新しい順」で上に積まれる。
*/
void CNoteBar::SaveOrder() const
{
	const std::wstring strPath = GetOrderFilePath();
	if( strPath.empty() ){
		return;
	}

	std::wstring strAll( 1, (WCHAR)0xFEFF );	// UTF-16LE の目印（BOM）。見えない字なので直に書かない
	strAll += L"# SakuraEditorPlus ノートバーの並び順\r\n";
	strAll += L"# ドラッグで並べ替えると自動で書き換わります。消すと「新しい順」に戻ります。\r\n";
	for( const SNote& note : m_vNotes ){
		strAll += NoteFileName( note.strPath );
		strAll += L"\r\n";
	}

	// 隠しファイルのままだと開き直せないので、いったん属性を戻してから書く
	::SetFileAttributes( strPath.c_str(), FILE_ATTRIBUTE_NORMAL );
	const HANDLE hFile = ::CreateFile( strPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
		nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
	if( INVALID_HANDLE_VALUE == hFile ){
		return;		// 書けなくても致命傷ではない（次に開いたら新しい順に戻るだけ）
	}
	DWORD dwWritten = 0;
	::WriteFile( hFile, strAll.c_str(), (DWORD)(strAll.size() * sizeof(WCHAR)), &dwWritten, nullptr );
	::CloseHandle( hFile );
	// メモではないので一覧に出さない（名前でも弾いているが、念のため隠しにもする）
	::SetFileAttributes( strPath.c_str(), FILE_ATTRIBUTE_HIDDEN );
}

/*! 覚え書きを捨てて「新しい順」に戻す */
void CNoteBar::ResetOrder()
{
	const std::wstring strPath = GetOrderFilePath();
	if( !strPath.empty() ){
		::SetFileAttributes( strPath.c_str(), FILE_ATTRIBUTE_NORMAL );
		::DeleteFile( strPath.c_str() );
	}
	m_vOrder.clear();
	Refresh( true );
	GetEditWnd().NotifyNoteBarChanged();
}

/*! 覚えた順に並べ替える

	覚え書きに載っていないファイル（＝あとから増えたメモ）は、
	新しい順のまま**先頭**へ置く。載っている物はその順で下に続く。
*/
void CNoteBar::ApplyOrder( std::vector<std::wstring>& vFiles ) const
{
	if( m_vOrder.empty() || vFiles.empty() ){
		return;
	}
	std::vector<std::wstring> vKnown;		// 覚えている物（覚えた順）
	std::vector<bool> vUsed( vFiles.size(), false );
	vKnown.reserve( vFiles.size() );
	for( const std::wstring& strName : m_vOrder ){
		for( size_t i = 0; i < vFiles.size(); ++i ){
			if( vUsed[i] ){
				continue;
			}
			if( 0 == _wcsicmp( strName.c_str(), NoteFileName( vFiles[i] ).c_str() ) ){
				vKnown.push_back( vFiles[i] );
				vUsed[i] = true;
				break;
			}
		}
	}
	if( vKnown.empty() ){
		return;
	}
	std::vector<std::wstring> vNew;			// あとから増えた物（新しい順のまま）
	vNew.reserve( vFiles.size() - vKnown.size() );
	for( size_t i = 0; i < vFiles.size(); ++i ){
		if( !vUsed[i] ){
			vNew.push_back( vFiles[i] );
		}
	}
	vFiles = vNew;
	vFiles.insert( vFiles.end(), vKnown.begin(), vKnown.end() );
}

void CNoteBar::Refresh( bool bForce )
{
	if( !GetHwnd() || !m_hwndList ){
		return;
	}
	if( IsCollapsed() ){
		return;		// 畳んでいる間は見えないので、フォルダーを見に行くだけ無駄
	}

	// フォルダーを見に行くのは軽くないので、立て続けに呼ばれたら省く
	const ULONGLONG ullNow = ::GetTickCount64();
	if( !bForce && ullNow - m_ullLastScan < 1000 ){
		SelectCurrentDocument();
		return;
	}
	m_ullLastScan = ullNow;

	// 🔥 Google ドライブは起動直後まだ生えていないことがある。
	//    そのまま一覧を作ると「メモが0件」に見えてしまうので、
	//    待ちだと分かるように一言出して、来るまで自分で見張る。
	if( IsFolderWaiting() ){
		ShowStatus( L"Google ドライブの接続待ち…", L"つながり次第ここに出ます" );
		if( !m_bWaitTimer && ::SetTimer( GetHwnd(), IDT_NOTEBAR_WAIT, 2000, nullptr ) ){
			m_bWaitTimer = true;
		}
		return;
	}
	if( m_bWaitTimer ){
		::KillTimer( GetHwnd(), IDT_NOTEBAR_WAIT );
		m_bWaitTimer = false;
	}
	if( !m_strStatus.empty() ){
		m_strStatus.clear();
		m_strStatusSub.clear();
		bForce = true;	// 待ちが明けた直後は必ず作り直す
	}

	const std::wstring strDir = GetNoteFolder();
	std::vector<std::wstring> vFiles = GetStashFilesInDir( strDir, NOTEBAR_MAX_ITEMS );

	// 自分で並べ替えたことがあれば、その順を優先する（覚え書きが無ければ新しい順のまま）
	LoadOrder();
	ApplyOrder( vFiles );

	// 中身が同じなら作り直さない（選択やスクロール位置を壊さないため）
	if( !bForce && vFiles.size() == m_vNotes.size() ){
		bool bSame = true;
		for( size_t i = 0; i < vFiles.size(); ++i ){
			if( 0 != _wcsicmp( vFiles[i].c_str(), m_vNotes[i].strPath.c_str() ) ){
				bSame = false;
				break;
			}
		}
		if( bSame ){
			SelectCurrentDocument();
			return;
		}
	}

	m_vNotes.clear();
	m_vNotes.reserve( vFiles.size() );
	for( const std::wstring& strPath : vFiles ){
		SNote note;
		note.strPath = strPath;
		SplitNoteName( strPath.c_str(), &note.strTitle, &note.strDate );
		m_vNotes.push_back( note );
	}

	::SendMessage( m_hwndList, WM_SETREDRAW, FALSE, 0 );
	::SendMessage( m_hwndList, LB_RESETCONTENT, 0, 0 );
	for( size_t i = 0; i < m_vNotes.size(); ++i ){
		const int nIdx = (int)::SendMessage( m_hwndList, LB_ADDSTRING, 0, (LPARAM)m_vNotes[i].strTitle.c_str() );
		if( 0 <= nIdx ){
			::SendMessage( m_hwndList, LB_SETITEMDATA, nIdx, (LPARAM)i );
		}
	}
	SelectCurrentDocument();
	::SendMessage( m_hwndList, WM_SETREDRAW, TRUE, 0 );
	::InvalidateRect( m_hwndList, nullptr, TRUE );
}

/*! 一覧の代わりに一言だけ出す

	「0件」と「まだ読めていない」は見た目が同じで区別がつかないので、
	理由をその場に出す。押しても何も起きない飾りの行にする。
*/
void CNoteBar::ShowStatus( LPCWSTR pszText, LPCWSTR pszSub )
{
	if( !m_hwndList ){
		return;
	}
	if( m_strStatus == pszText && m_strStatusSub == pszSub ){
		return;		// もう出ている（見張りタイマーで何度も来るのでちらつかせない）
	}
	m_strStatus    = pszText;
	m_strStatusSub = pszSub;
	m_vNotes.clear();
	::SendMessage( m_hwndList, WM_SETREDRAW, FALSE, 0 );
	::SendMessage( m_hwndList, LB_RESETCONTENT, 0, 0 );
	const int nIdx = (int)::SendMessage( m_hwndList, LB_ADDSTRING, 0, (LPARAM)pszText );
	if( 0 <= nIdx ){
		::SendMessage( m_hwndList, LB_SETITEMDATA, nIdx, (LPARAM)NOTEBAR_STATUS_ITEM );
	}
	::SendMessage( m_hwndList, LB_SETCURSEL, (WPARAM)-1, 0 );
	::SendMessage( m_hwndList, WM_SETREDRAW, TRUE, 0 );
	::InvalidateRect( m_hwndList, nullptr, TRUE );
}

/*! 今開いている文書を選択状態にする */
void CNoteBar::SelectCurrentDocument()
{
	if( !m_hwndList ){
		return;
	}
	const CEditDoc* pcDoc = GetEditWnd().GetDocument();
	if( !pcDoc || !pcDoc->m_cDocFile.GetFilePathClass().IsValidPath() ){
		::SendMessage( m_hwndList, LB_SETCURSEL, (WPARAM)-1, 0 );
		return;
	}
	const std::wstring strPath = pcDoc->m_cDocFile.GetFilePath();
	for( size_t i = 0; i < m_vNotes.size(); ++i ){
		if( 0 == _wcsicmp( strPath.c_str(), m_vNotes[i].strPath.c_str() ) ){
			::SendMessage( m_hwndList, LB_SETCURSEL, (WPARAM)i, 0 );
			return;
		}
	}
	::SendMessage( m_hwndList, LB_SETCURSEL, (WPARAM)-1, 0 );
}

void CNoteBar::LayoutChildren()
{
	if( !GetHwnd() || !m_hwndList ){
		return;
	}
	if( IsCollapsed() ){
		::ShowWindow( m_hwndList, SW_HIDE );
		return;
	}
	RECT rc;
	::GetClientRect( GetHwnd(), &rc );
	const int nHeader = ::DpiScaleY( NOTEBAR_HEADER_H );
	const int nGrip   = ::DpiScaleX( NOTEBAR_GRIP_W );
	const int nW = rc.right - rc.left - nGrip;
	const int nH = rc.bottom - rc.top - nHeader;
	::MoveWindow( m_hwndList, 0, nHeader, (nW > 0)? nW: 0, (nH > 0)? nH: 0, TRUE );
	::ShowWindow( m_hwndList, SW_SHOWNA );
}

/*! 親に配置し直してもらう */
void CNoteBar::NotifyParentLayout()
{
	GetEditWnd().RelayoutClientArea();
}

LRESULT CNoteBar::OnSize( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	LayoutChildren();
	return 0L;
}

LRESULT CNoteBar::OnPaint( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	PAINTSTRUCT ps;
	const HDC hdc = ::BeginPaint( hwnd, &ps );

	RECT rc;
	::GetClientRect( hwnd, &rc );

	const bool     bDark    = IsDarkModeActive();
	const COLORREF clrBack  = bDark ? DarkMode::getDlgBackgroundColor() : ::GetSysColor( COLOR_WINDOW );
	const COLORREF clrBtn   = bDark ? DarkMode::getHotBackgroundColor() : ::GetSysColor( COLOR_BTNFACE );
	const COLORREF clrEdge  = bDark ? DarkMode::getEdgeColor()          : ::GetSysColor( COLOR_3DSHADOW );
	const COLORREF clrText  = bDark ? DarkMode::getTextColor()          : ::GetSysColor( COLOR_BTNTEXT );

	const int nHeader = ::DpiScaleY( NOTEBAR_HEADER_H );
	const int nGrip   = ::DpiScaleX( NOTEBAR_GRIP_W );

	// 畳んであるときは細い帯だけ。帯ぜんぶが「▶（開く）」ボタン。
	// 本文と同じ色だと「ただの余白」に見えて押せると気づけないので、ボタン色で塗る。
	if( IsCollapsed() ){
		auto blend = []( COLORREF a, COLORREF b ){
			return RGB( (GetRValue(a) + GetRValue(b)) / 2,
						(GetGValue(a) + GetGValue(b)) / 2,
						(GetBValue(a) + GetBValue(b)) / 2 );
		};
		::MyFillRect( hdc, rc, m_bCloseDown ? clrEdge : (m_bCloseHot ? blend( clrBtn, clrEdge ) : clrBtn) );
		RECT rcLine = rc;
		rcLine.left = rcLine.right - ::DpiScaleX( 1 );
		::MyFillRect( hdc, rcLine, clrEdge );
		RECT rcBtn = { rc.left, rc.top, rc.right - ::DpiScaleX( 1 ), rc.top + nHeader };
		DrawChevron( hdc, rcBtn, clrText, true, 3 );
		::EndPaint( hwnd, &ps );
		return 0L;
	}

	// 上の帯ぜんぶが「◀（畳む）」ボタン。印は帯の真ん中に置く
	RECT rcHeader;
	GetHeaderRect( &rcHeader );
	::MyFillRect( hdc, rcHeader, m_bCloseDown ? clrEdge : (m_bCloseHot ? clrBtn : clrBack) );
	{
		RECT rcLine = rcHeader;
		rcLine.top = rcLine.bottom - ::DpiScaleY( 1 );
		::MyFillRect( hdc, rcLine, clrEdge );
	}
	// 「◀」＝畳む。× ではなく向きのある印にして、畳んだ帯の「▶」と対にする
	DrawChevron( hdc, rcHeader, clrText, false, 4 );

	// 右端の掴む所
	RECT rcGrip = { rc.right - nGrip, rc.top, rc.right, rc.bottom };
	::MyFillRect( hdc, rcGrip, clrBack );
	{
		RECT rcLine = rcGrip;
		rcLine.left = rcLine.right - ::DpiScaleX( 1 );
		::MyFillRect( hdc, rcLine, clrEdge );
	}

	// 一覧が無いときの余白
	RECT rcRest = { rc.left, rc.top + nHeader, rc.right - nGrip, rc.bottom };
	if( m_vNotes.empty() ){
		::MyFillRect( hdc, rcRest, clrBack );
		const int nSave = ::SaveDC( hdc );
		::SelectObject( hdc, m_hFontSub );
		::SetBkMode( hdc, TRANSPARENT );
		::SetTextColor( hdc, clrEdge );
		RECT rcText = rcRest;
		rcText.top  += ::DpiScaleY( 12 );
		rcText.left += ::DpiScaleX( NOTEBAR_PAD );
		rcText.right -= ::DpiScaleX( NOTEBAR_PAD );
		::DrawText( hdc, L"まだメモがありません", -1, &rcText,
			DT_WORDBREAK | DT_NOPREFIX );
		::RestoreDC( hdc, nSave );
	}

	::EndPaint( hwnd, &ps );
	return 0L;
}

LRESULT CNoteBar::OnMeasureItem( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	MEASUREITEMSTRUCT* pmis = (MEASUREITEMSTRUCT*)lParam;
	if( pmis && ODT_LISTBOX == pmis->CtlType && 0 < m_nItemHeight ){
		pmis->itemHeight = m_nItemHeight;
		return TRUE;
	}
	return CWnd::OnMeasureItem( hwnd, uMsg, wParam, lParam );
}

LRESULT CNoteBar::OnDrawItem( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	DRAWITEMSTRUCT* pdis = (DRAWITEMSTRUCT*)lParam;
	if( !pdis || ODT_LISTBOX != pdis->CtlType || (UINT)-1 == pdis->itemID ){
		return CWnd::OnDrawItem( hwnd, uMsg, wParam, lParam );
	}
	const bool bDarkNow    = IsDarkModeActive();
	const COLORREF clrBase = bDarkNow ? DarkMode::getDlgBackgroundColor() : ::GetSysColor( COLOR_WINDOW );
	const COLORREF clrGray = bDarkNow ? DarkMode::getEdgeColor()          : ::GetSysColor( COLOR_3DSHADOW );
	if( NOTEBAR_STATUS_ITEM == pdis->itemData ){
		// 一覧の代わりに出している一言（接続待ちなど）。選ばせない・押させない
		::MyFillRect( pdis->hDC, pdis->rcItem, clrBase );
		const int nSaveSt = ::SaveDC( pdis->hDC );
		::SetBkMode( pdis->hDC, TRANSPARENT );
		::SetTextColor( pdis->hDC, clrGray );
		RECT rcSt = pdis->rcItem;
		rcSt.left  += ::DpiScaleX( NOTEBAR_PAD + 3 );
		rcSt.right -= ::DpiScaleX( NOTEBAR_PAD );
		rcSt.top   += ::DpiScaleY( 4 );
		::SelectObject( pdis->hDC, m_hFontTitle );
		TEXTMETRIC tmSt;
		::GetTextMetrics( pdis->hDC, &tmSt );
		RECT rcStTitle = rcSt;
		rcStTitle.bottom = rcStTitle.top + tmSt.tmHeight;
		::DrawText( pdis->hDC, m_strStatus.c_str(), -1, &rcStTitle,
			DT_SINGLELINE | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX );
		if( !m_strStatusSub.empty() ){
			::SelectObject( pdis->hDC, m_hFontSub );
			RECT rcStSub = rcSt;
			rcStSub.top = rcStTitle.bottom + ::DpiScaleY( 1 );
			::DrawText( pdis->hDC, m_strStatusSub.c_str(), -1, &rcStSub,
				DT_SINGLELINE | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX );
		}
		::RestoreDC( pdis->hDC, nSaveSt );
		return TRUE;
	}
	const size_t nNote = (size_t)pdis->itemData;
	if( m_vNotes.size() <= nNote ){
		return TRUE;
	}
	const SNote& note = m_vNotes[nNote];

	const bool bSelected = (0 != (pdis->itemState & ODS_SELECTED));
	const bool bDark     = IsDarkModeActive();
	const COLORREF clrBack   = bDark ? DarkMode::getDlgBackgroundColor() : ::GetSysColor( COLOR_WINDOW );
	const COLORREF clrSelBk  = bDark ? DarkMode::getHotBackgroundColor() : ::GetSysColor( COLOR_BTNFACE );
	const COLORREF clrAccent = ::GetSysColor( COLOR_HIGHLIGHT );
	const COLORREF clrText   = bDark ? DarkMode::getTextColor()          : ::GetSysColor( COLOR_WINDOWTEXT );
	const COLORREF clrSub    = bDark ? DarkMode::getEdgeColor()          : ::GetSysColor( COLOR_3DSHADOW );

	// 掴んで動かしている1件。どれを持っているか分かるようにする
	const bool bGrabbed = ( m_bDragging && (int)nNote == m_nDragFrom );

	RECT rc = pdis->rcItem;
	::MyFillRect( pdis->hDC, rc, (bSelected || bGrabbed) ? clrSelBk : clrBack );
	if( bSelected ){
		// 今開いているノート＝左端に帯
		RECT rcAccent = rc;
		rcAccent.right = rcAccent.left + ::DpiScaleX( 3 );
		::MyFillRect( pdis->hDC, rcAccent, clrAccent );
	}
	{
		RECT rcLine = rc;
		rcLine.top = rcLine.bottom - ::DpiScaleY( 1 );
		::MyFillRect( pdis->hDC, rcLine, clrBack );
	}
	if( bGrabbed ){
		const HBRUSH hbrFrame = ::CreateSolidBrush( clrAccent );
		if( hbrFrame ){
			::FrameRect( pdis->hDC, &rc, hbrFrame );
			::DeleteObject( hbrFrame );
		}
	}

	const int nSave = ::SaveDC( pdis->hDC );
	::SetBkMode( pdis->hDC, TRANSPARENT );

	RECT rcText = rc;
	rcText.left  += ::DpiScaleX( NOTEBAR_PAD + 3 );
	rcText.right -= ::DpiScaleX( NOTEBAR_PAD );
	rcText.top   += ::DpiScaleY( 4 );

	// 見出し
	::SelectObject( pdis->hDC, m_hFontTitle );
	::SetTextColor( pdis->hDC, clrText );
	TEXTMETRIC tm;
	::GetTextMetrics( pdis->hDC, &tm );
	RECT rcTitle = rcText;
	rcTitle.bottom = rcTitle.top + tm.tmHeight;
	::DrawText( pdis->hDC, note.strTitle.c_str(), -1, &rcTitle,
		DT_SINGLELINE | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX );

	// 日時
	if( !note.strDate.empty() ){
		::SelectObject( pdis->hDC, m_hFontSub );
		::SetTextColor( pdis->hDC, clrSub );
		RECT rcDate = rcText;
		rcDate.top = rcTitle.bottom + ::DpiScaleY( 1 );
		::DrawText( pdis->hDC, note.strDate.c_str(), -1, &rcDate,
			DT_SINGLELINE | DT_LEFT | DT_END_ELLIPSIS | DT_NOPREFIX );
	}

	::RestoreDC( pdis->hDC, nSave );
	return TRUE;
}

LRESULT CNoteBar::OnCommand( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if( m_hwndList && (HWND)lParam == m_hwndList && LBN_SELCHANGE == HIWORD( wParam ) ){
		const int nSel = (int)::SendMessage( m_hwndList, LB_GETCURSEL, 0, 0 );
		if( 0 <= nSel ){
			// 一覧の処理中に開くと入れ子になるので、いったん抜けてから開く
			::PostMessage( GetHwnd(), WM_NOTEBAR_OPEN, (WPARAM)nSel, 0 );
		}
		return 0L;
	}
	return CWnd::OnCommand( hwnd, uMsg, wParam, lParam );
}

LRESULT CNoteBar::DispatchEvent_WM_APP( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if( WM_NOTEBAR_OPEN == uMsg ){
		OpenNote( (int)wParam );
		return 0L;
	}
	return CWnd::DispatchEvent_WM_APP( hwnd, uMsg, wParam, lParam );
}

LRESULT CNoteBar::DispatchEvent( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	switch( uMsg ){
	case WM_SETCURSOR:
		if( (HWND)wParam == hwnd ){
			POINT pt;
			::GetCursorPos( &pt );
			::ScreenToClient( hwnd, &pt );
			if( IsInGrip( pt ) || m_bSizing ){
				::SetCursor( ::LoadCursor( nullptr, IDC_SIZEWE ) );
				return TRUE;
			}
			if( IsInCloseBtn( pt ) ){
				// 帯ぜんぶが開閉ボタンなので、押せると分かるようにする
				::SetCursor( ::LoadCursor( nullptr, IDC_HAND ) );
				return TRUE;
			}
		}
		break;
	case WM_CONTEXTMENU:
		{
			POINT pt = { (short)LOWORD( lParam ), (short)HIWORD( lParam ) };
			int nIndex = -1;
			if( -1 == pt.x && -1 == pt.y ){
				// キーボードのアプリケーションキー
				RECT rcWnd;
				::GetWindowRect( hwnd, &rcWnd );
				pt.x = rcWnd.left + 20;
				pt.y = rcWnd.top + 40;
				if( m_hwndList ){
					nIndex = (int)::SendMessage( m_hwndList, LB_GETCURSEL, 0, 0 );
				}
			}else if( m_hwndList ){
				POINT ptList = pt;
				::ScreenToClient( m_hwndList, &ptList );
				nIndex = HitTestList( ptList );
			}
			ShowNoteMenu( nIndex, pt );
		}
		return 0L;
	case WM_TIMER:
		if( IDT_NOTEBAR_WAIT == (UINT_PTR)wParam ){
			// ドライブが来たか見に行く。来ていれば Refresh の中で見張りを止める
			Refresh( true );
			return 0L;
		}
		break;
	case WM_MOUSELEAVE:
		m_bTracking = false;
		if( m_bCloseHot ){
			m_bCloseHot = false;
			::InvalidateRect( hwnd, nullptr, FALSE );
		}
		return 0L;
	case WM_CTLCOLORLISTBOX:
		if( m_hbrBack ){
			const bool bDark = IsDarkModeActive();
			::SetBkColor( (HDC)wParam, bDark ? DarkMode::getDlgBackgroundColor() : ::GetSysColor( COLOR_WINDOW ) );
			::SetTextColor( (HDC)wParam, bDark ? DarkMode::getTextColor() : ::GetSysColor( COLOR_WINDOWTEXT ) );
			return (LRESULT)m_hbrBack;
		}
		break;
	default:
		break;
	}
	return CWnd::DispatchEvent( hwnd, uMsg, wParam, lParam );
}

/*! 「◀」「▶」を線で描く

	字ではなく線で描く（フォントに無い記号を選んで豆腐になるのを避ける）。

	@param bToRight true なら ▶（開く）、false なら ◀（畳む）
	@param nArmRaw  大きさ（DPI補正前・横の半分の長さ。縦はこの2倍）
*/
void CNoteBar::DrawChevron( HDC hdc, const RECT& rcBtn, COLORREF clrLine, bool bToRight, int nArmRaw ) const
{
	const int nSave = ::SaveDC( hdc );
	const int nArm  = ::DpiScaleX( nArmRaw );
	const int cxMid = (rcBtn.left + rcBtn.right) / 2;
	const int cyMid = (rcBtn.top + rcBtn.bottom) / 2;
	const int nTip  = bToRight ? (cxMid + nArm) : (cxMid - nArm);
	const int nEnd  = bToRight ? (cxMid - nArm) : (cxMid + nArm);
	const HPEN hPen = ::CreatePen( PS_SOLID, ::DpiScaleX( 2 ), clrLine );
	HPEN hOld = (HPEN)::SelectObject( hdc, hPen );
	::MoveToEx( hdc, nEnd, cyMid - nArm * 2, nullptr );
	::LineTo(   hdc, nTip, cyMid );
	::LineTo(   hdc, nEnd, cyMid + nArm * 2 );
	::SelectObject( hdc, hOld );
	::DeleteObject( hPen );
	::RestoreDC( hdc, nSave );
}

bool CNoteBar::IsInGrip( POINT ptClient ) const
{
	if( IsCollapsed() ){
		return false;	// 畳んでいるときは幅を変えられない（帯ぜんぶが開くボタン）
	}
	RECT rc;
	::GetClientRect( GetHwnd(), &rc );
	return ( ptClient.x >= rc.right - ::DpiScaleX( NOTEBAR_GRIP_W ) );
}

/*! 上の帯（＝畳むボタン）の位置を出す

	@param[out] pRcHeader 帯全体（右端の掴む所は含めない）
*/
void CNoteBar::GetHeaderRect( RECT* pRcHeader ) const
{
	RECT rc;
	::GetClientRect( GetHwnd(), &rc );
	RECT rcHeader = { rc.left, rc.top, rc.right - ::DpiScaleX( NOTEBAR_GRIP_W ),
					  rc.top + ::DpiScaleY( NOTEBAR_HEADER_H ) };
	if( pRcHeader ) *pRcHeader = rcHeader;
}

//! 開閉ボタンの上か。開いているときは上の帯ぜんぶ、畳んでいるときは細い帯ぜんぶ
bool CNoteBar::IsInCloseBtn( POINT ptClient ) const
{
	if( IsCollapsed() ){
		RECT rc;
		::GetClientRect( GetHwnd(), &rc );
		return ( FALSE != ::PtInRect( &rc, ptClient ) );
	}
	RECT rcHeader;
	GetHeaderRect( &rcHeader );
	return ( FALSE != ::PtInRect( &rcHeader, ptClient ) );
}

int CNoteBar::HitTestList( POINT ptClient ) const
{
	if( !m_hwndList ){
		return -1;
	}
	const DWORD dwRet = (DWORD)::SendMessage( m_hwndList, LB_ITEMFROMPOINT, 0,
		MAKELPARAM( ptClient.x, ptClient.y ) );
	if( 0 != HIWORD( dwRet ) ){
		return -1;	// 項目の外
	}
	const int nIndex = (int)LOWORD( dwRet );
	return ( 0 <= nIndex && (size_t)nIndex < m_vNotes.size() ) ? nIndex : -1;
}

/*! どこへ差し込むか（0〜件数）

	1件の高さは固定（LBS_OWNERDRAWFIXED）なので、いちばん上に出ている位置から数えられる。
	項目の上半分なら「その手前」、下半分なら「その次」＝掴んだ物を置く場所。
*/
int CNoteBar::HitTestInsert( POINT ptList ) const
{
	const int nCount = (int)m_vNotes.size();
	if( nCount <= 0 || !m_hwndList || m_nItemHeight <= 0 ){
		return 0;
	}
	const int nTop = (int)::SendMessage( m_hwndList, LB_GETTOPINDEX, 0, 0 );
	// 上へはみ出したとき（y が負）も切り捨ての向きがずれないようにする
	const int nRel = ( 0 <= ptList.y )
		? ( ptList.y / m_nItemHeight )
		: -( ( -ptList.y + m_nItemHeight - 1 ) / m_nItemHeight );
	const int nFrac = ptList.y - nRel * m_nItemHeight;
	int nInsert = nTop + nRel + ( ( nFrac > m_nItemHeight / 2 ) ? 1 : 0 );
	if( nInsert < 0 )      nInsert = 0;
	if( nInsert > nCount ) nInsert = nCount;
	return nInsert;
}

/*! 差し込み位置が変わったら描き直す */
void CNoteBar::SetDropAt( int nDropAt )
{
	if( nDropAt == m_nDropAt ){
		return;
	}
	m_nDropAt = nDropAt;
	if( m_hwndList ){
		::InvalidateRect( m_hwndList, nullptr, FALSE );
	}
}

/*! 差し込み位置の線を引く（項目を描いたあとに重ねる） */
void CNoteBar::DrawInsertMark( HDC hdc ) const
{
	if( !m_bDragging || m_nDropAt < 0 || !m_hwndList || m_nItemHeight <= 0 ){
		return;
	}
	RECT rc;
	::GetClientRect( m_hwndList, &rc );
	const int nTop   = (int)::SendMessage( m_hwndList, LB_GETTOPINDEX, 0, 0 );
	const int nThick = ::DpiScaleY( 2 );
	int y = ( m_nDropAt - nTop ) * m_nItemHeight;
	if( y < rc.top )               y = rc.top;
	if( y > rc.bottom - nThick )   y = rc.bottom - nThick;
	RECT rcMark = { rc.left, y, rc.right, y + nThick };
	::MyFillRect( hdc, rcMark, ::GetSysColor( COLOR_HIGHLIGHT ) );
}

/*! 端まで持っていったときに一覧を送る

	マウスを動かさずに端で止めていても送りたいので、タイマーから呼ぶ。
*/
void CNoteBar::AutoScrollForDrag()
{
	if( !m_bDragging || !m_hwndList || m_nItemHeight <= 0 ){
		return;
	}
	POINT pt;
	::GetCursorPos( &pt );
	::ScreenToClient( m_hwndList, &pt );
	RECT rc;
	::GetClientRect( m_hwndList, &rc );
	const int nEdge = m_nItemHeight / 2;
	const int nTop  = (int)::SendMessage( m_hwndList, LB_GETTOPINDEX, 0, 0 );
	int nNewTop = nTop;
	if( pt.y < rc.top + nEdge ){
		nNewTop = nTop - 1;
	}else if( pt.y > rc.bottom - nEdge ){
		nNewTop = nTop + 1;
	}
	if( nNewTop != nTop ){
		if( nNewTop < 0 ) nNewTop = 0;
		if( nNewTop > (int)m_vNotes.size() - 1 ) nNewTop = (int)m_vNotes.size() - 1;
		if( nNewTop != nTop ){
			::SendMessage( m_hwndList, LB_SETTOPINDEX, (WPARAM)nNewTop, 0 );
			::InvalidateRect( m_hwndList, nullptr, FALSE );
		}
	}
	SetDropAt( HitTestInsert( pt ) );
}

/*! 掴んでいる状態を終える

	@param bApply true なら並べ替えを反映する（離した）／false なら捨てる（中断）
	@note 状態を先に消してから ReleaseCapture する。
	      解放すると WM_CAPTURECHANGED でここへ戻ってくるので、そのとき何も起きないようにするため。
*/
void CNoteBar::EndDrag( bool bApply )
{
	const bool bWasDragging = m_bDragging;
	const int  nFrom        = m_nDragFrom;
	const int  nDropAt      = m_nDropAt;

	if( m_bDragScroll && m_hwndList ){
		::KillTimer( m_hwndList, IDT_NOTEBAR_DRAG );
	}
	m_bDragScroll = false;
	m_bMayDrag    = false;
	m_bDragging   = false;
	m_nDragFrom   = -1;
	m_nDropAt     = -1;

	if( m_hwndList && ::GetCapture() == m_hwndList ){
		::ReleaseCapture();
	}
	if( m_hwndList && bWasDragging ){
		::InvalidateRect( m_hwndList, nullptr, FALSE );
	}
	if( bApply && bWasDragging && 0 <= nFrom && 0 <= nDropAt ){
		MoveNote( nFrom, nDropAt );
	}
}

/*! ドラッグの結果を反映する

	@param nFrom     掴んだ位置
	@param nInsertAt 差し込む位置（0〜件数。「その手前に入れる」という意味）
*/
void CNoteBar::MoveNote( int nFrom, int nInsertAt )
{
	const int nCount = (int)m_vNotes.size();
	if( nFrom < 0 || nFrom >= nCount ){
		return;
	}
	if( nInsertAt < 0 )      nInsertAt = 0;
	if( nInsertAt > nCount ) nInsertAt = nCount;
	// 自分より下へ入れるときは、自分が抜けるぶん1つ詰まる
	const int nTo = ( nInsertAt > nFrom ) ? nInsertAt - 1 : nInsertAt;
	if( nTo == nFrom ){
		return;		// 動いていない
	}

	const SNote note = m_vNotes[nFrom];
	m_vNotes.erase( m_vNotes.begin() + nFrom );
	m_vNotes.insert( m_vNotes.begin() + nTo, note );

	SaveOrder();
	// 🔥 覚え書きを書いたら、必ず読み直した結果で画面を作り直す。
	//    書けていなければ元の並びに戻る＝画面と実物が食い違ったままにならない。
	Refresh( true );
	GetEditWnd().NotifyNoteBarChanged();	// 他の窓の一覧も同じ並びにする
}

/*! 一覧（リストボックス）の横取り

	並べ替えのために左ボタンを自分で見る。
	🔥 押した時点では何もしない（開かない・選択も変えない）。
	   ここで既定の処理へ流すと LBN_SELCHANGE が出て**押しただけで開いて**しまい、
	   掴んで動かすことができなくなる。開くのは「動かさずに離した」ときだけ。
*/
LRESULT CALLBACK CNoteBar::ListProc( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
	UINT_PTR uIdSubclass, DWORD_PTR dwRefData )
{
	CNoteBar* pThis = (CNoteBar*)dwRefData;
	if( !pThis ){
		return ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
	}

	switch( uMsg ){
	case WM_LBUTTONDOWN:
		{
			POINT pt = { (short)LOWORD( lParam ), (short)HIWORD( lParam ) };
			const int nIndex = pThis->HitTestList( pt );
			if( nIndex < 0 ){
				break;		// 項目の外＝いつも通り
			}
			pThis->m_bMayDrag    = true;
			pThis->m_bDragging   = false;
			pThis->m_nDragFrom   = nIndex;
			pThis->m_nDropAt     = -1;
			pThis->m_ptDragStart = pt;
			::SetCapture( hwnd );
		}
		return 0L;

	case WM_LBUTTONDBLCLK:
		// 1回目のクリックでもう開いている。既定へ流すと選択が動くだけなので握りつぶす
		return 0L;

	case WM_MOUSEMOVE:
		if( pThis->m_bMayDrag ){
			POINT pt = { (short)LOWORD( lParam ), (short)HIWORD( lParam ) };
			if( !pThis->m_bDragging ){
				// 少し動かすまでは「クリック」として扱う（押しただけで並びが変わらないように）
				const int nDx = abs( pt.x - pThis->m_ptDragStart.x );
				const int nDy = abs( pt.y - pThis->m_ptDragStart.y );
				if( nDx < ::GetSystemMetrics( SM_CXDRAG ) && nDy < ::GetSystemMetrics( SM_CYDRAG ) ){
					return 0L;
				}
				pThis->m_bDragging = true;
				if( ::SetTimer( hwnd, IDT_NOTEBAR_DRAG, NOTEBAR_DRAG_SCROLL_MS, nullptr ) ){
					pThis->m_bDragScroll = true;
				}
				::SetCursor( ::LoadCursor( nullptr, IDC_SIZENS ) );
				::InvalidateRect( hwnd, nullptr, FALSE );
			}
			pThis->SetDropAt( pThis->HitTestInsert( pt ) );
			return 0L;
		}
		break;

	case WM_SETCURSOR:
		if( pThis->m_bDragging ){
			::SetCursor( ::LoadCursor( nullptr, IDC_SIZENS ) );
			return TRUE;
		}
		break;

	case WM_TIMER:
		if( IDT_NOTEBAR_DRAG == (UINT_PTR)wParam ){
			pThis->AutoScrollForDrag();
			return 0L;
		}
		break;

	case WM_LBUTTONUP:
		if( pThis->m_bMayDrag ){
			const bool bWasDragging = pThis->m_bDragging;
			const int  nFrom        = pThis->m_nDragFrom;
			pThis->EndDrag( true );
			if( !bWasDragging && 0 <= nFrom ){
				// 動かさずに離した＝ただのクリック。一覧の処理中に開くと入れ子になるので投げる
				::PostMessage( pThis->GetHwnd(), WM_NOTEBAR_OPEN, (WPARAM)nFrom, 0 );
			}
			return 0L;
		}
		break;

	case WM_CAPTURECHANGED:
		pThis->EndDrag( false );
		break;

	case WM_PAINT:
		{
			// 項目を先に描いてもらってから、差し込み位置の線を上に重ねる
			const LRESULT lRes = ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
			if( pThis->m_bDragging ){
				const HDC hdc = ::GetDC( hwnd );
				if( hdc ){
					pThis->DrawInsertMark( hdc );
					::ReleaseDC( hwnd, hdc );
				}
			}
			return lRes;
		}

	default:
		break;
	}
	return ::DefSubclassProc( hwnd, uMsg, wParam, lParam );
}

LRESULT CNoteBar::OnLButtonDown( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	POINT pt = { (short)LOWORD( lParam ), (short)HIWORD( lParam ) };
	if( IsInGrip( pt ) ){
		RECT rc;
		::GetClientRect( hwnd, &rc );
		m_bSizing       = true;
		m_nSizeOrgX     = pt.x;
		m_nSizeOrgWidth = rc.right - rc.left;
		::SetCapture( hwnd );
		return 0L;
	}
	if( IsInCloseBtn( pt ) ){
		m_bCloseDown = true;
		::SetCapture( hwnd );
		::InvalidateRect( hwnd, nullptr, FALSE );
		return 0L;
	}
	return 0L;
}

LRESULT CNoteBar::OnMouseMove( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	POINT pt = { (short)LOWORD( lParam ), (short)HIWORD( lParam ) };

	if( m_bSizing ){
		const int nWidth = m_nSizeOrgWidth + (pt.x - m_nSizeOrgX);
		// 記憶するのは DPI 補正前の値（別解像度の画面へ移しても崩れないように）
		int nStore = ::MulDiv( nWidth, 96, ::DpiScaleX( 96 ) );
		if( nStore < NOTEBAR_MIN_W ) nStore = NOTEBAR_MIN_W;
		if( nStore > NOTEBAR_MAX_W ) nStore = NOTEBAR_MAX_W;
		if( nStore != GetDllShareData().m_Common.m_sWindow.m_nNoteBarWidth ){
			GetDllShareData().m_Common.m_sWindow.m_nNoteBarWidth = nStore;
			NotifyParentLayout();
		}
		return 0L;
	}

	if( !m_bTracking ){
		TRACKMOUSEEVENT tme;
		tme.cbSize      = sizeof(tme);
		tme.dwFlags     = TME_LEAVE;
		tme.hwndTrack   = hwnd;
		tme.dwHoverTime = 0;
		if( ::TrackMouseEvent( &tme ) ){
			m_bTracking = true;
		}
	}
	const bool bCloseHot = IsInCloseBtn( pt );
	if( bCloseHot != m_bCloseHot ){
		m_bCloseHot = bCloseHot;
		::InvalidateRect( hwnd, nullptr, FALSE );
	}
	return 0L;
}

LRESULT CNoteBar::OnLButtonUp( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	POINT pt = { (short)LOWORD( lParam ), (short)HIWORD( lParam ) };

	if( m_bSizing ){
		m_bSizing = false;
		::ReleaseCapture();
		// 幅は全ウィンドウ共通の設定なので、他の窓にも配置し直してもらう
		GetEditWnd().NotifyNoteBarChanged();
		return 0L;
	}
	if( m_bCloseDown ){
		m_bCloseDown = false;
		::ReleaseCapture();
		::InvalidateRect( hwnd, nullptr, FALSE );
		if( IsInCloseBtn( pt ) ){
			// 閉じる＝表示の切り替えコマンドに任せる（設定の保存と他の窓への通知もやってくれる）
			::PostMessageAny( GetParentHwnd(), WM_COMMAND, MAKEWPARAM( F_SHOWNOTEBAR, 0 ), (LPARAM)nullptr );
		}
		return 0L;
	}
	return 0L;
}

LRESULT CNoteBar::OnDestroy( HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam )
{
	if( m_bWaitTimer ){
		::KillTimer( hwnd, IDT_NOTEBAR_WAIT );
		m_bWaitTimer = false;
	}
	// 子より先に親の WM_DESTROY が来るので、ここならまだ一覧は生きている
	if( m_hwndList && ::IsWindow( m_hwndList ) ){
		EndDrag( false );
		::RemoveWindowSubclass( m_hwndList, &ListProc, NOTEBAR_LIST_SUBCLASS );
	}
	m_hwndList = nullptr;
	return 0L;
}

/*! ノートを開く */
void CNoteBar::OpenNote( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_vNotes.size() ){
		return;
	}
	const std::wstring strPath = m_vNotes[nIndex].strPath;
	if( !fexist( strPath.c_str() ) ){
		// 一覧が古い（外で消された）
		Refresh( true );
		return;
	}

	CEditDoc* pcDoc = GetEditWnd().GetDocument();
	if( pcDoc && pcDoc->m_cDocFile.GetFilePathClass().IsValidPath()
	 && 0 == _wcsicmp( strPath.c_str(), pcDoc->m_cDocFile.GetFilePath() ) ){
		// 既にこの窓で開いている
		::SetFocus( GetEditWnd().GetActiveView().GetHwnd() );
		return;
	}

	// 🔥 開いたノートのタブへ焦点まで移す。
	//    以前は無条件に FileLoad → 自分の窓へ SetFocus していたので、
	//    別のタブ（＝別の窓）に開かれたときに焦点を自分へ奪い返してしまい、
	//    「選んだのにタブが切り替わらない」状態になっていた。
	//    タグジャンプ（CEditView_Command.cpp）と同じ順序に合わせる。
	HWND hwndOwner = nullptr;
	if( !CShareData::getInstance()->IsPathOpened( strPath.c_str(), &hwndOwner ) ){
		// まだどこにも開いていない
		if( pcDoc && pcDoc->IsAcceptLoad() ){
			// この窓が空（無題・未編集）なので、ここに読む＝タブを増やさない
			SLoadInfo sLoadInfo( strPath.c_str(), CODE_AUTODETECT, false );
			pcDoc->m_cDocFileOperation.FileLoad( &sLoadInfo );
			::SetFocus( GetEditWnd().GetActiveView().GetHwnd() );
			SelectCurrentDocument();
			return;
		}
		// この窓は埋まっているので新しいタブで開く（sync=true で起動を待つ）
		SLoadInfo sLoadInfo( strPath.c_str(), CODE_AUTODETECT, false );
		CControlTray::OpenNewEditor( G_AppInstance(), GetHwnd(), sLoadInfo, nullptr, true );
		if( !CShareData::getInstance()->IsPathOpened( strPath.c_str(), &hwndOwner ) ){
			return;	// 開けなかった（メッセージは開く側が出している）
		}
	}
	if( hwndOwner ){
		ActivateFrameWindow( hwndOwner );
	}
}

/*! 右クリックメニュー */
void CNoteBar::ShowNoteMenu( int nIndex, POINT ptScreen )
{
	const HMENU hMenu = ::CreatePopupMenu();
	if( !hMenu ){
		return;
	}
	const bool bHasItem = ( 0 <= nIndex && (size_t)nIndex < m_vNotes.size() );
	if( bHasItem ){
		::AppendMenu( hMenu, MF_STRING, NOTEMENU_OPEN,   L"開く(&O)" );
		::AppendMenu( hMenu, MF_STRING, NOTEMENU_RENAME, L"名前の変更(&M)..." );
		::AppendMenu( hMenu, MF_STRING, NOTEMENU_DELETE, L"削除(&D)" );
		::AppendMenu( hMenu, MF_SEPARATOR, 0, nullptr );
		::AppendMenu( hMenu, MF_STRING, NOTEMENU_REVEAL, L"エクスプローラーで表示(&E)" );
	}
	if( bHasItem ){
		::AppendMenu( hMenu, MF_SEPARATOR, 0, nullptr );
	}
	// 自分で並べ替えたあと、元の「新しい順」へ戻す道を必ず残しておく
	::AppendMenu( hMenu, MF_STRING | (m_vOrder.empty() ? MF_GRAYED : 0),
		NOTEMENU_RESETORDER, L"並び順を新しい順に戻す(&R)" );
	::AppendMenu( hMenu, MF_STRING, NOTEMENU_FOLDER, L"ノートのフォルダーを開く(&F)" );

	const int nCmd = ::TrackPopupMenu( hMenu,
		TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_RIGHTBUTTON,
		ptScreen.x, ptScreen.y, 0, GetHwnd(), nullptr );
	::DestroyMenu( hMenu );

	switch( nCmd ){
	case NOTEMENU_OPEN:		OpenNote( nIndex );		break;
	case NOTEMENU_RENAME:	RenameNote( nIndex );	break;
	case NOTEMENU_DELETE:	DeleteNote( nIndex );	break;
	case NOTEMENU_REVEAL:	RevealNote( nIndex );	break;
	case NOTEMENU_RESETORDER:
		if( IDYES == ::MessageBox( GetHwnd(),
				L"自分で並べ替えた順を捨てて、新しい順に戻します。\nよろしいですか？",
				L"並び順を戻す", MB_YESNO | MB_ICONQUESTION ) ){
			ResetOrder();
		}
		break;
	case NOTEMENU_FOLDER:
		{
			const std::wstring strDir = GetNoteFolder();
			if( !strDir.empty() ){
				::ShellExecute( GetHwnd(), L"open", strDir.c_str(), nullptr, nullptr, SW_SHOWNORMAL );
			}
		}
		break;
	default:
		break;
	}
}

/*! 名前を変える

	開いている文書なら、編集中の内容ごと改名する既存の機能（F_FILE_RENAME）に任せる。
	開いていないファイルは、ここでファイル名だけ変える。
*/
void CNoteBar::RenameNote( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_vNotes.size() ){
		return;
	}
	const std::wstring strOldPath = m_vNotes[nIndex].strPath;

	CEditDoc* pcDoc = GetEditWnd().GetDocument();
	if( pcDoc && pcDoc->m_cDocFile.GetFilePathClass().IsValidPath()
	 && 0 == _wcsicmp( strOldPath.c_str(), pcDoc->m_cDocFile.GetFilePath() ) ){
		::PostMessageAny( GetParentHwnd(), WM_COMMAND, MAKEWPARAM( F_FILE_RENAME, 0 ), (LPARAM)nullptr );
		return;
	}

	WCHAR szFolder[_MAX_PATH] = L"";
	WCHAR szOldName[_MAX_PATH] = L"";
	SplitPath_FolderAndFile( strOldPath.c_str(), szFolder, szOldName );

	WCHAR szName[_MAX_PATH];
	wcscpy_s( szName, szOldName );
	auto& cDlgInput1 = *CDlgInput1::getInstance();
	if( !cDlgInput1.DoModal( G_AppInstance(), GetHwnd(), L"ファイル名の変更",
			L"新しいファイル名(&N)", _MAX_PATH - 1, szName ) ){
		return;
	}

	std::wstring strName = szName;
	while( !strName.empty() && (L' ' == strName.front() || L'\t' == strName.front()) ){
		strName.erase( strName.begin() );
	}
	while( !strName.empty() && (L' ' == strName.back() || L'\t' == strName.back() || L'.' == strName.back()) ){
		strName.pop_back();
	}
	if( strName.empty() ){
		return;
	}
	if( std::wstring::npos != strName.find_first_of( L"\\/:*?\"<>|" ) ){
		ErrorMessage( GetHwnd(), L"ファイル名に使えない文字が入っています。\n\\ / : * ? \" < > | は使えません。" );
		return;
	}
	if( std::wstring::npos == strName.find( L'.' ) ){
		const WCHAR* pszExt = wcsrchr( szOldName, L'.' );
		strName += ( nullptr != pszExt ) ? pszExt : L".txt";
	}

	std::wstring strNewPath = szFolder;
	if( !strNewPath.empty() && L'\\' != strNewPath.back() ){
		strNewPath += L'\\';
	}
	strNewPath += strName;

	if( 0 == _wcsicmp( strNewPath.c_str(), strOldPath.c_str() ) ){
		return;
	}
	if( fexist( strNewPath.c_str() ) ){
		ErrorMessage( GetHwnd(), L"同じ名前のファイルが既にあります。\n%ls", strName.c_str() );
		return;
	}
	if( !::MoveFile( strOldPath.c_str(), strNewPath.c_str() ) ){
		ErrorMessage( GetHwnd(), L"名前を変更できませんでした。\n%ls", strOldPath.c_str() );
		return;
	}
	// 自分で並べ替えているなら、覚え書きの名前も差し替える。
	// ここを忘れると、名前を変えたとたんに「知らないメモ」扱いで一番上へ飛ぶ。
	if( !m_vOrder.empty() ){
		m_vNotes[nIndex].strPath = strNewPath;
		SaveOrder();
	}
	Refresh( true );
	GetEditWnd().NotifyNoteBarChanged();	// 他の窓の一覧にも新しい名前を届ける
}

/*! 消す（ごみ箱へ入れるので取り戻せる） */
void CNoteBar::DeleteNote( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_vNotes.size() ){
		return;
	}
	const std::wstring strPath = m_vNotes[nIndex].strPath;

	// 開いたままだと「消したのにタブに残っている」状態になるので、一緒に閉じる
	HWND hwndOpened = nullptr;
	if( !CShareData::getInstance()->IsPathOpened( strPath.c_str(), &hwndOpened ) ){
		hwndOpened = nullptr;
	}

	std::wstring strMsg = L"「";
	strMsg += m_vNotes[nIndex].strTitle;
	strMsg += L"」をごみ箱へ移動します。";
	if( hwndOpened ){
		strMsg += L"\n開いているタブも閉じます。";
	}
	strMsg += L"\nよろしいですか？";
	if( IDYES != ::MessageBox( GetHwnd(), strMsg.c_str(), L"ノートの削除", MB_YESNO | MB_ICONQUESTION ) ){
		return;
	}

	// SHFileOperation は文字列の終わりに \0 を2つ要求する
	std::vector<WCHAR> vFrom( strPath.begin(), strPath.end() );
	vFrom.push_back( L'\0' );
	vFrom.push_back( L'\0' );

	SHFILEOPSTRUCT fos;
	::ZeroMemory( &fos, sizeof(fos) );
	fos.hwnd   = GetHwnd();
	fos.wFunc  = FO_DELETE;
	fos.pFrom  = &vFrom[0];
	fos.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT;
	if( 0 != ::SHFileOperation( &fos ) || fos.fAnyOperationsAborted ){
		ErrorMessage( GetHwnd(), L"削除できませんでした。\n%ls", strPath.c_str() );
		Refresh( true );
		return;
	}

	// 覚え書きからも外す（放っておいても読み飛ばされるが、消したものを溜めない）
	if( !m_vOrder.empty() ){
		m_vNotes.erase( m_vNotes.begin() + nIndex );
		SaveOrder();
	}

	// 消せたときだけタブを閉じる。
	// ⚠ 自分がぶら下がっている窓のこともあるので、必ず Post（同期で呼ぶと自分を壊す）。
	if( hwndOpened && ::IsWindow( hwndOpened ) ){
		// 最後の1枚だったら窓ごと消さない。消すとサイドバーごと画面から無くなって
		// 「削除したらエディタが終了した」ように見えるため、中身だけ(無題)に戻す。
		EditNode* pEditNode = nullptr;
		const int nOpened = CAppNodeManager::getInstance()->GetOpenedWindowArr( &pEditNode, FALSE );
		delete[] pEditNode;
		if( nOpened <= 1 ){
			::PostMessage( hwndOpened, WM_COMMAND, MAKEWPARAM( F_FILECLOSE, 0 ), (LPARAM)nullptr );
		}else{
			// メニューの「閉じる」と同じ道（タブまとめ表示なら次のタブへ移ってから閉じる）
			::PostMessage( hwndOpened, MYWM_CLOSE, 0,
				(LPARAM)CAppNodeManager::getInstance()->GetNextTab( hwndOpened ) );
		}
	}
	Refresh( true );
	// 🔥 他の窓の一覧も作り直させる。ここを忘れると、別のタブには
	//    消したはずのノートが残り続け、それを押して「開けない」と怒られる。
	GetEditWnd().NotifyNoteBarChanged();
}

/*! エクスプローラーで場所を開く */
void CNoteBar::RevealNote( int nIndex )
{
	if( nIndex < 0 || (size_t)nIndex >= m_vNotes.size() ){
		return;
	}
	std::wstring strParam = L"/select,\"";
	strParam += m_vNotes[nIndex].strPath;
	strParam += L"\"";
	::ShellExecute( GetHwnd(), L"open", L"explorer.exe", strParam.c_str(), nullptr, SW_SHOWNORMAL );
}
