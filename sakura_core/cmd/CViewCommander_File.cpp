/*!	@file
@brief CViewCommanderクラスのコマンド(ファイル操作系)関数群

	2012/12/20	CViewCommander.cppから分離
*/
/*
	Copyright (C) 1998-2001, Norio Nakatani
	Copyright (C) 2000-2001, jepro
	Copyright (C) 2002, YAZAKI, genta
	Copyright (C) 2003, MIK, genta, かろと, Moca
	Copyright (C) 2004, genta
	Copyright (C) 2005, genta
	Copyright (C) 2006, ryoji, maru
	Copyright (C) 2007, ryoji, maru, genta
	Copyright (C) 2018-2026, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/

#include "StdAfx.h"
#include "CViewCommander.h"
#include "CViewCommander_inline.h"

#include "_main/CControlTray.h"
#include "uiparts/CWaitCursor.h"
#include "dlg/CDlgProperty.h"
#include "dlg/CDlgCancel.h"// 2002/2/8 hor
#include "dlg/CDlgProfileMgr.h"
#include "doc/CDocReader.h"	//  Command_PROPERTY_FILE for _DEBUG
#include "print/CPrintPreview.h"
#include "io/CBinaryStream.h"
#include "io/CFileLoad.h"
#include "io/CTextStream.h"	// 【自前改造】クイック退避で使う
#include "util/quickstash.h"	// 【自前改造】退避フォルダーの決定（メニュー側と共用）
#include "util/updater.h"	// 【自前改造】自動更新／手動更新
#include "env/CWriteManager.h"
#include "CEditApp.h"
#include "recent/CMRUFile.h"
#include "util/shell.h"
#include "util/window.h"
#include "charset/CCodeFactory.h"
#include "plugin/CPlugin.h"
#include "plugin/CJackManager.h"
#include "env/CSakuraEnvironment.h"
#include "debug/CRunningTimer.h"
#include "util/os.h"
#include "apiwrap/CommonControl.h"
#include "CSelectLang.h"
#include "sakura_rc.h"
#include "config/app_constants.h"

// ============================================================================
// 【自前改造】クイック退避（MY_MODS.md 参照）
//
//   「名前を決めて・保存場所を決めて」が重いので、無題の文書はダイアログを出さずに
//   Google ドライブの退避フォルダーへ保存する。名前は「日時＋先頭行」で自動生成する。
// ============================================================================

namespace {

// GetQuickStashDir() は util/quickstash.h（メニュー側と共用）

//! 先頭の中身のある行から、ファイル名に使える短い見出しを作る
/*!
	@param[in] cDocLineMgr 対象の文書
	@return 見出し文字列（30文字まで）。作れなければ空文字列
	@note ファイル名に使えない文字と前後の空白は落とす
*/
std::wstring MakeQuickStashTitle( const CDocLineMgr& cDocLineMgr )
{
	const int MAX_TITLE_LEN = 30;
	std::wstring strTitle;

	for( const CDocLine* pcDocLine = cDocLineMgr.GetDocLineTop();
		 nullptr != pcDocLine && strTitle.empty();
		 pcDocLine = pcDocLine->GetNextLine() )
	{
		const wchar_t*  pLine = pcDocLine->GetPtr();
		const int       nLen  = (Int)pcDocLine->GetLengthWithoutEOL();
		for( int i = 0; i < nLen && (int)strTitle.length() < MAX_TITLE_LEN; ++i ){
			const wchar_t c = pLine[i];
			// ファイル名に使えない文字・制御文字は捨てる
			if( c < L' ' || nullptr != wcschr( L"\\/:*?\"<>|", c ) ){
				continue;
			}
			// 行頭の空白は見出しに入れない
			if( strTitle.empty() && (L' ' == c || L'\t' == c) ){
				continue;
			}
			strTitle += ( L'\t' == c ) ? L' ' : c;
		}
	}

	// 末尾の空白とピリオドを落とす（Windows がファイル名として嫌がるため）
	while( !strTitle.empty() ){
		const wchar_t c = strTitle[strTitle.length() - 1];
		if( L' ' != c && L'.' != c ){
			break;
		}
		strTitle.erase( strTitle.length() - 1 );
	}
	return strTitle;
}

//! 退避先のフルパスを作る（まだ存在しない名前を返す）
/*!
	@param[in] cDocLineMgr 対象の文書（先頭行から見出しを作るのに使う）
	@return 退避先のフルパス。用意できなかったときは空文字列
*/
std::wstring MakeQuickStashPath( const CDocLineMgr& cDocLineMgr )
{
	const std::wstring strDir = GetQuickStashDir();
	if( strDir.empty() ){
		return std::wstring();
	}

	// 日時 → 「2026-08-21_1240」
	SYSTEMTIME systime;
	::GetLocalTime( &systime );
	WCHAR szStamp[32];
	::auto_sprintf_s( szStamp, _countof(szStamp), L"%04d-%02d-%02d_%02d%02d",
		systime.wYear, systime.wMonth, systime.wDay, systime.wHour, systime.wMinute );

	const std::wstring strTitle = MakeQuickStashTitle( cDocLineMgr );

	// 同じ分に何度も退避したとき用に連番で逃がす
	for( int nSeq = 0; nSeq < 100; ++nSeq ){
		std::wstring strName = szStamp;
		if( !strTitle.empty() ){
			strName += L" ";
			strName += strTitle;
		}
		if( 0 < nSeq ){
			WCHAR szSeq[16];
			::auto_sprintf_s( szSeq, _countof(szSeq), L"(%d)", nSeq + 1 );
			strName += szSeq;
		}
		strName += L".txt";

		std::wstring strPath = strDir + L"\\" + strName;
		if( !fexist( strPath.c_str() ) ){
			return strPath;
		}
	}
	return std::wstring();
}

} // namespace

/* 新規作成 */
void CViewCommander::Command_FILENEW( void )
{
	/* 新たな編集ウィンドウを起動 */
	SLoadInfo sLoadInfo;
	sLoadInfo.cFilePath = L"";
	sLoadInfo.eCharCode = CODE_NONE;
	sLoadInfo.bViewMode = false;
	std::wstring curDir = CSakuraEnvironment::GetDlgInitialDir();
	CControlTray::OpenNewEditor( G_AppInstance(), m_pCommanderView->GetHwnd(), sLoadInfo, nullptr, false, curDir.c_str(), false );
	return;
}

/* 新規作成（新しいウインドウで開く） */
void CViewCommander::Command_FILENEW_NEWWINDOW( void )
{
	/* 新たな編集ウィンドウを起動 */
	SLoadInfo sLoadInfo;
	sLoadInfo.cFilePath = L"";
	sLoadInfo.eCharCode = CODE_DEFAULT;
	sLoadInfo.bViewMode = false;
	std::wstring curDir = CSakuraEnvironment::GetDlgInitialDir();
	CControlTray::OpenNewEditor( G_AppInstance(), m_pCommanderView->GetHwnd(), sLoadInfo,
		nullptr,
		false,
		curDir.c_str(),
		true
		);
	return;
}

/*! @brief ファイルを開く

	@date 2003.03.30 genta 「閉じて開く」から利用するために引数追加
	@date 2004.10.09 genta 実装をCEditDocへ移動
*/
void CViewCommander::Command_FILEOPEN( const WCHAR* filename, ECodeType nCharCode, bool bViewMode, const WCHAR* defaultName )
{
	if( !IsValidCodeType(nCharCode) && nCharCode != CODE_AUTODETECT ){
		nCharCode = CODE_AUTODETECT;
	}
	//ロード情報
	SLoadInfo sLoadInfo(filename?filename:L"", nCharCode, bViewMode);
	std::vector<std::wstring> files;

	//必要であれば「ファイルを開く」ダイアログ
	if(!sLoadInfo.cFilePath.IsValidPath()){
		std::wstring defName = (defaultName ? defaultName : L"");
		if( !defName.empty() ){
			WCHAR szPath[_MAX_PATH];
			WCHAR szDir[_MAX_DIR];
			WCHAR szName[_MAX_FNAME];
			WCHAR szExt  [_MAX_EXT];
			my_splitpath_t(defName.c_str(), szPath, szDir, szName, szExt);
			wcscat(szPath, szDir);
			if( 0 == wmemicmp(defName.c_str(), szPath) ){
				// defNameはフォルダー名だった
			}else{
				CFilePath path = defName.c_str();
				if( 0 == wmemicmp(path.GetDirPath().c_str(), szPath) ){
					// フォルダー名までは実在している
					sLoadInfo.cFilePath = defName.c_str();
				}
			}
		}
		bool bDlgResult = GetDocument()->m_cDocFileOperation.OpenFileDialog(
			CEditWnd::getInstance()->GetHwnd(),	//[in]  オーナーウィンドウ
			defName.length()==0 ? nullptr : defName.c_str(),	//[in]  フォルダー
			&sLoadInfo,							//[out] ロード情報受け取り
			files								//[out] ファイル名
		);
		if(!bDlgResult)return;

		for(size_t i = 0; i < files.size(); i++ ){
			if (files[i].length() >= _MAX_PATH){
				ErrorMessage(
					CEditWnd::getInstance()->GetHwnd(),
					LS(STR_ERR_FILEPATH_TOO_LONG),
					files[i].c_str()
				);
				return;
			}
		}
		sLoadInfo.cFilePath = files[0].c_str();
		// 他のファイルは新規ウィンドウ
		int nSize = (int)files.size();
		for( int i = 1; i < nSize; i++ ){
			SLoadInfo sFilesLoadInfo = sLoadInfo;
			sFilesLoadInfo.cFilePath = files[i].c_str();
			CControlTray::OpenNewEditor(
				G_AppInstance(),
				CEditWnd::getInstance()->GetHwnd(),
				sFilesLoadInfo,
				nullptr,
				true
			);
		}
	}

	//開く
	GetDocument()->m_cDocFileOperation.FileLoad( &sLoadInfo );
}

/*! 上書き保存

	F_FILESAVEALLとの組み合わせのみで使われるコマンド．
	@param warnbeep [in] true: 保存不要 or 保存禁止のときに警告を出す
	@param askname	[in] true: ファイル名未設定の時に入力を促す

	@date 2004.02.28 genta 引数warnbeep追加
	@date 2005.01.24 genta 引数askname追加

*/
bool CViewCommander::Command_FILESAVE( bool warnbeep, bool askname )
{
	CEditDoc* pcDoc = GetDocument();

	//ファイル名が指定されていない場合
	if( !GetDocument()->m_cDocFile.GetFilePathClass().IsValidPath() ){
		if( !askname )
			return false;	// 保存しない
		// 【自前改造】無題の文書は、名前も場所も聞かずに退避フォルダー（Googleドライブ）へ保存する。
		//             「名前を決めて・保存場所を決めて」を無くすのが目的（MY_MODS.md 参照）。
		//             明示的に置き場所を選びたいときは「名前を付けて保存」を使う。
		std::wstring strStashPath = MakeQuickStashPath( GetDocument()->m_cDocLineMgr );
		if( !strStashPath.empty() ){
			if( pcDoc->m_cDocFileOperation.FileSaveAs( strStashPath.c_str(), CODE_NONE, EEolType::none, false ) ){
				std::wstring strMsg = L"退避しました: ";
				strMsg += strStashPath;
				m_pCommanderView->SendStatusMessage( strMsg.c_str() );
				return true;
			}
		}
		// 退避先が用意できなかったときは、従来どおりダイアログに逃がす
		return pcDoc->m_cDocFileOperation.FileSaveAs();
	}

	//セーブ情報
	SSaveInfo sSaveInfo;
	pcDoc->GetSaveInfo(&sSaveInfo);
	sSaveInfo.cEol = EEolType::none; //改行コード無変換
	sSaveInfo.bOverwriteMode = true; //上書き要求

	//上書き処理
	if(!warnbeep)CEditApp::getInstance()->m_cSoundSet.MuteOn();
	bool bRet = pcDoc->m_cDocFileOperation.DoSaveFlow(&sSaveInfo);
	if(!warnbeep)CEditApp::getInstance()->m_cSoundSet.MuteOff();

	return bRet;
}

/* 名前を付けて保存ダイアログ */
bool CViewCommander::Command_FILESAVEAS_DIALOG(const WCHAR* fileNameDef,ECodeType eCodeType, EEolType eEolType)
{
	return 	GetDocument()->m_cDocFileOperation.FileSaveAs(fileNameDef, eCodeType, eEolType, true);
}

/* 名前を付けて保存
	filenameで保存。NULLは厳禁。
*/
BOOL CViewCommander::Command_FILESAVEAS( const WCHAR* filename, EEolType eEolType )
{
	return 	GetDocument()->m_cDocFileOperation.FileSaveAs(filename, CODE_NONE, eEolType, false);
}

/*!	全て上書き保存

	編集中の全てのウィンドウで上書き保存を行う．
	ただし，上書き保存の指示を出すのみで実行結果の確認は行わない．

	上書き禁止及びファイル名未設定のウィンドウでは何も行わない．

	@date 2005.01.24 genta 新規作成
*/
BOOL CViewCommander::Command_FILESAVEALL( void )
{
	CAppNodeGroupHandle(0).SendMessageToAllEditors(
		WM_COMMAND,
		MAKELONG( F_FILESAVE_QUIET, 0 ),
		0,
		nullptr
	);
	return TRUE;
}

/* 閉じて(無題) */	//Oct. 17, 2000 jepro 「ファイルを閉じる」というキャプションを変更
void CViewCommander::Command_FILECLOSE( void )
{
	GetDocument()->m_cDocFileOperation.FileClose();
}

/*! @brief 閉じて開く

	@date 2003.03.30 genta 開くダイアログでキャンセルしたとき元のファイルが残るように。
				ついでにFILEOPENと同じように引数を追加しておく
*/
void CViewCommander::Command_FILECLOSE_OPEN( LPCWSTR filename, ECodeType nCharCode, bool bViewMode )
{
	GetDocument()->m_cDocFileOperation.FileCloseOpen( SLoadInfo(filename, nCharCode, bViewMode) );

	//プラグイン：DocumentOpenイベント実行
	CJackManager::getInstance()->InvokePlugins( PP_DOCUMENT_OPEN, &GetEditWindow()->GetActiveView() );
}

//! ファイルの再オープン
void CViewCommander::Command_FILE_REOPEN(
	ECodeType	nCharCode,	//!< [in] 開き直す際の文字コード
	bool		bNoConfirm	//!< [in] ファイルが更新された場合に確認を行わ「ない」かどうか。true:確認しない false:確認する
)
{
	CEditDoc* pcDoc = GetDocument();
	if( !bNoConfirm && fexist(pcDoc->m_cDocFile.GetFilePath()) && pcDoc->m_cDocEditor.IsModified() ){
		int nDlgResult = MYMESSAGEBOX(
			m_pCommanderView->GetHwnd(),
			MB_OKCANCEL | MB_ICONQUESTION | MB_TOPMOST,
			GSTR_APPNAME,
			LS(STR_ERR_CEDITVIEW_CMD29),
			pcDoc->m_cDocFile.GetFilePath()
		);
		if( IDOK == nDlgResult ){
			//継続。下へ進む
		}else{
			return; //中断
		}
	}

	// 同一ファイルの再オープン
	pcDoc->m_cDocFileOperation.ReloadCurrentFile( nCharCode );
}

/* 印刷 */
void CViewCommander::Command_PRINT( void )
{
	// 使っていない処理を削除 2003.05.04 かろと
	Command_PRINT_PREVIEW();

	/* 印刷実行 */
	GetEditWindow()->m_pPrintPreview->OnPrint();
}

/* 印刷プレビュー */
void CViewCommander::Command_PRINT_PREVIEW( void )
{
	/* 印刷プレビューモードのオン/オフ */
	GetEditWindow()->PrintPreviewModeONOFF();
	return;
}

/* 印刷のページレイアウトの設定 */
void CViewCommander::Command_PRINT_PAGESETUP( void )
{
	/* 印刷ページ設定 */
	GetEditWindow()->OnPrintPageSetting();
	return;
}

//From Here Feb. 10, 2001 JEPRO 追加
/* C/C++ヘッダーファイルまたはソースファイル オープン機能 */
BOOL CViewCommander::Command_OPEN_HfromtoC( BOOL bCheckOnly )
{
	if ( Command_OPEN_HHPP( bCheckOnly, FALSE ) )	return TRUE;
	if ( Command_OPEN_CCPP( bCheckOnly, FALSE ) )	return TRUE;
	if (!bCheckOnly) ErrorBeep();
	return FALSE;
// 2002/03/24 YAZAKI コードの重複を削減
// 2003.06.28 Moca コメントとして残っていたコードを削除
}

/* C/C++ヘッダーファイル オープン機能 */		//Feb. 10, 2001 jepro	説明を「インクルードファイル」から変更
//BOOL CViewCommander::Command_OPENINCLUDEFILE( BOOL bCheckOnly )
BOOL CViewCommander::Command_OPEN_HHPP( BOOL bCheckOnly, BOOL bBeepWhenMiss )
{
	// 2003.06.28 Moca ヘッダー・ソースのコードを統合＆削除
	static const WCHAR* source_ext[] = { L"c", L"cpp", L"cxx", L"cc", L"cp", L"c++" };
	static const WCHAR* header_ext[] = { L"h", L"hpp", L"hxx", L"hh", L"hp", L"h++" };
	return m_pCommanderView->OPEN_ExtFromtoExt(
		bCheckOnly, bBeepWhenMiss, source_ext, header_ext,
		int(std::size(source_ext)), int(std::size(header_ext)),
		LS(STR_ERR_CEDITVIEW_CMD08) );
}

/* C/C++ソースファイル オープン機能 */
//BOOL CViewCommander::Command_OPENCCPP( BOOL bCheckOnly )	//Feb. 10, 2001 JEPRO	コマンド名を若干変更
BOOL CViewCommander::Command_OPEN_CCPP( BOOL bCheckOnly, BOOL bBeepWhenMiss )
{
	// 2003.06.28 Moca ヘッダー・ソースのコードを統合＆削除
	static const WCHAR* source_ext[] = { L"c", L"cpp", L"cxx", L"cc", L"cp", L"c++" };
	static const WCHAR* header_ext[] = { L"h", L"hpp", L"hxx", L"hh", L"hp", L"h++" };
	return m_pCommanderView->OPEN_ExtFromtoExt(
		bCheckOnly, bBeepWhenMiss, header_ext, source_ext,
		int(std::size(header_ext)), int(std::size(source_ext)),
		LS(STR_ERR_CEDITVIEW_CMD09));
}

/* Oracle SQL*Plusをアクティブ表示 */
void CViewCommander::Command_ACTIVATE_SQLPLUS( void )
{
	HWND		hwndSQLPLUS;
	hwndSQLPLUS = ::FindWindow( L"SqlplusWClass", L"Oracle SQL*Plus" );
	if( nullptr == hwndSQLPLUS ){
		ErrorMessage( m_pCommanderView->GetHwnd(), LS( STR_SQLERR_ACTV_BUT_NOT_RUN ) );	//"Oracle SQL*Plusをアクティブ表示します。\n\n\nOracle SQL*Plusが起動されていません。\n"
		return;
	}
	/* Oracle SQL*Plusをアクティブにする */
	/* アクティブにする */
	ActivateFrameWindow( hwndSQLPLUS );
	return;
}

/* Oracle SQL*Plusで実行 */
void CViewCommander::Command_PLSQL_COMPILE_ON_SQLPLUS( void )
{
//	HGLOBAL		hgClip;
//	char*		pszClip;
	HWND		hwndSQLPLUS;
	int			nRet;
	BOOL		nBool;
	WCHAR		szPath[MAX_PATH + 2];
	BOOL		bResult;

	hwndSQLPLUS = ::FindWindow( L"SqlplusWClass", L"Oracle SQL*Plus" );
	if( nullptr == hwndSQLPLUS ){
		ErrorMessage( m_pCommanderView->GetHwnd(), LS( STR_SQLERR_EXEC_BUT_NOT_RUN ) );	//"Oracle SQL*Plusで実行します。\n\n\nOracle SQL*Plusが起動されていません。\n"
		return;
	}
	/* テキストが変更されている場合 */
	if( GetDocument()->m_cDocEditor.IsModified() ){
		nRet = ::MYMESSAGEBOX(
			m_pCommanderView->GetHwnd(),
			MB_YESNOCANCEL | MB_ICONEXCLAMATION,
			GSTR_APPNAME,
			LS(STR_ERR_CEDITVIEW_CMD18),
			GetDocument()->m_cDocFile.GetFilePathClass().IsValidPath() ? GetDocument()->m_cDocFile.GetFilePath() : LS(STR_NO_TITLE1)
		);
		switch( nRet ){
		case IDYES:
			if( GetDocument()->m_cDocFile.GetFilePathClass().IsValidPath() ){
				//nBool = HandleCommand( F_FILESAVE, true, 0, 0, 0, 0 );
				nBool = Command_FILESAVE();
			}else{
				//nBool = HandleCommand( F_FILESAVEAS_DIALOG, true, 0, 0, 0, 0 );
				nBool = Command_FILESAVEAS_DIALOG(nullptr, CODE_NONE, EEolType::none);
			}
			if( !nBool ){
				return;
			}
			break;
		case IDNO:
			return;
		case IDCANCEL:
		default:
			return;
		}
	}
	if( GetDocument()->m_cDocFile.GetFilePathClass().IsValidPath() ){
		/* ファイルパスに空白が含まれている場合はダブルクォーテーションで囲む */
		//	2003.10.20 MIK コード簡略化
		if( wcschr( GetDocument()->m_cDocFile.GetFilePath(), WCODE::SPACE ) ? TRUE : FALSE ){
			auto_sprintf( szPath, L"@\"%s\"\r\n", GetDocument()->m_cDocFile.GetFilePath() );
		}else{
			auto_sprintf( szPath, L"@%s\r\n", GetDocument()->m_cDocFile.GetFilePath() );
		}
		/* クリップボードにデータを設定 */
		m_pCommanderView->MySetClipboardData( szPath, wcslen( szPath ), false );

		/* Oracle SQL*Plusをアクティブにする */
		/* アクティブにする */
		ActivateFrameWindow( hwndSQLPLUS );

		/* Oracle SQL*Plusにペーストのコマンドを送る */
		DWORD_PTR	dwResult;
		bResult = (BOOL)::SendMessageTimeoutW(
			hwndSQLPLUS,
			WM_COMMAND,
			MAKELONG( 201, 0 ),
			0,
			SMTO_ABORTIFHUNG | SMTO_NORMAL,
			3000,
			&dwResult
		);
		if( !bResult ){
			TopErrorMessage( m_pCommanderView->GetHwnd(), LS(STR_ERR_CEDITVIEW_CMD20) );
		}
	}else{
		ErrorBeep();
		ErrorMessage( m_pCommanderView->GetHwnd(), LS(STR_ERR_CEDITVIEW_CMD21) );
		return;
	}
	return;
}

/* ブラウズ */
void CViewCommander::Command_BROWSE( void )
{
	if( !GetDocument()->m_cDocFile.GetFilePathClass().IsValidPath() ){
		ErrorBeep();
		return;
	}
//	char	szURL[MAX_PATH + 64];
//	auto_sprintf( szURL, L"%ls", GetDocument()->m_cDocFile.GetFilePath() );
	/* URLを開く */
//	::ShellExecuteEx( NULL, L"open", szURL, NULL, NULL, SW_SHOW );

    SHELLEXECUTEINFO info; 
    info.cbSize =sizeof(info);
    info.fMask = 0;
    info.hwnd = nullptr;
    info.lpVerb = nullptr;
    info.lpFile = GetDocument()->m_cDocFile.GetFilePath();
    info.lpParameters = nullptr;
    info.lpDirectory = nullptr;
    info.nShow = SW_SHOWNORMAL;
    info.hInstApp = nullptr;
    info.lpIDList = nullptr;
    info.lpClass = nullptr;
    info.hkeyClass = nullptr;
    info.dwHotKey = 0;
    info.hIcon = nullptr;

	::ShellExecuteEx(&info);

	return;
}

/* ビューモード */
void CViewCommander::Command_VIEWMODE( void )
{
	//ビューモードを反転
	CAppMode::getInstance()->SetViewMode(!CAppMode::getInstance()->IsViewMode());

	// 排他制御の切り替え
	// ※ビューモード ON 時は排他制御 OFF、ビューモード OFF 時は排他制御 ON の仕様（>>data:5262）を即時反映する
	GetDocument()->m_cDocFileOperation.DoFileUnlock();	// ファイルの排他ロック解除
	GetDocument()->m_cDocLocker.CheckWritable(!CAppMode::getInstance()->IsViewMode());	// ファイル書込可能のチェック
	if( GetDocument()->m_cDocLocker.IsDocWritable() ){
		GetDocument()->m_cDocFileOperation.DoFileLock();	// ファイルの排他ロック
	}

	// 親ウィンドウのタイトルを更新
	GetEditWindow()->UpdateCaption();
}

/* ファイルのプロパティ */
void CViewCommander::Command_PROPERTY_FILE( void )
{
#ifdef _DEBUG
	{
		/* 全行データを返すテスト */
		wchar_t*	pDataAll;
		int		nDataAllLen;
		CRunningTimer cRunningTimer( L"CViewCommander::Command_PROPERTY_FILE 全行データを返すテスト" );
		cRunningTimer.Reset();
		pDataAll = CDocReader(GetDocument()->m_cDocLineMgr).GetAllData( &nDataAllLen );
//		MYTRACE( L"全データ取得             (%dバイト) 所要時間(ミリ秒) = %d\n", nDataAllLen, cRunningTimer.Read() );
		free( pDataAll );
		pDataAll = nullptr;
//		MYTRACE( L"全データ取得のメモリ解放 (%dバイト) 所要時間(ミリ秒) = %d\n", nDataAllLen, cRunningTimer.Read() );
	}
#endif

	CDlgProperty	cDlgProperty;
//	cDlgProperty.Create( G_AppInstance(), m_pCommanderView->GetHwnd(), GetDocument() );
	cDlgProperty.DoModal( G_AppInstance(), m_pCommanderView->GetHwnd(), (LPARAM)GetDocument() );
	return;
}

void CViewCommander::Command_PROFILEMGR( void )
{
	CDlgProfileMgr profMgr;
	if( profMgr.DoModal( G_AppInstance(), m_pCommanderView->GetHwnd(), 0 ) ){
		WCHAR szOpt[MAX_PATH+10];
		auto_sprintf( szOpt, L"-PROF=\"%s\"", profMgr.m_strProfileName.c_str() );
		SLoadInfo sLoadInfo;
		sLoadInfo.cFilePath = L"";
		sLoadInfo.eCharCode = CODE_DEFAULT;
		sLoadInfo.bViewMode = false;
		CControlTray::OpenNewEditor( G_AppInstance(), m_pCommanderView->GetHwnd(), sLoadInfo, szOpt, false, nullptr, false );
	}
}

/* ファイルの場所を開く */
void CViewCommander::Command_OPEN_FOLDER_IN_EXPLORER(void)
{
	if (!GetDocument()->m_cDocFile.GetFilePathClass().IsValidPath()) {
		ErrorBeep();
		return;
	}

	// ドキュメントパスを変数に入れる
	LPCWSTR pszDocPath = GetDocument()->m_cDocFile.GetFilePath();

	// Windows Explorerの引数を作る
	CNativeW explorerCommand;
	explorerCommand.AppendStringF(L"/select,\"%s\"", pszDocPath);
	LPCWSTR pszExplorerCommand = explorerCommand.GetStringPtr();

	auto hInstance = ::ShellExecute(GetMainWindow(), L"open", L"explorer.exe", pszExplorerCommand, nullptr, SW_SHOWNORMAL);
	// If the function succeeds, it returns a value greater than 32. 
	if (hInstance <= (decltype(hInstance))32) {
		ErrorBeep();
		return;
	}

	return;
}

/* コマンドプロンプトを開く */
void CViewCommander::Command_OPEN_COMMAND_PROMPT(BOOL isAdmin)
{
	if (!GetDocument()->m_cDocFile.GetFilePathClass().IsValidPath()) {
		ErrorBeep();
		return;
	}
	
	/* UNC パスに対してコマンドプロンプトを開けないので弾く */
	if (PathIsUNCW(GetDocument()->m_cDocFile.GetFilePath())) {
		ErrorBeep();
		return;
	}

	std::wstring strFolder(GetDocument()->m_cDocFile.GetFilePathClass().GetDirPath());

	/* 環境変数 COMSPEC から cmd.exe のパスを取得する */
	SFilePath szCmdExePathBuf;
	if (::GetEnvironmentVariableW(L"COMSPEC", szCmdExePathBuf, int(std::size(szCmdExePathBuf))) == 0) {
		ErrorBeep();
		return;
	}

	LPCWSTR pVerb = L"open";
	if (isAdmin)
	{
		pVerb = L"runas";
	}

#ifndef _WIN64
	/*
		64bit OS で 32bit アプリからコマンドプロンプトを起動する場合
		通常は 32bit 版のコマンドプロンプトが開かれる。

		Wow64 の FileSystem Redirection を一時的にオフにすることにより 64bit 版の
		コマンドプロンプトを起動する
	*/
	CDisableWow64FsRedirect wow64Redirect(TRUE);
#endif

	SHELLEXECUTEINFOW execInfo{ sizeof(SHELLEXECUTEINFOW) };
	execInfo.fMask = SEE_MASK_DEFAULT;
	execInfo.lpVerb = pVerb;
	execInfo.lpFile = szCmdExePathBuf;
	execInfo.lpParameters = nullptr;	// 対話モードにするためパラメーターを指定しない
	execInfo.lpDirectory = std::data(strFolder);
	execInfo.nShow = SW_SHOWNORMAL;

	if (!Shell32::getInstance()->ShellExecuteExW(&execInfo)) {
		ErrorBeep();
		return;
	}
}

/* PowerShellを開く */
void CViewCommander::Command_OPEN_POWERSHELL(BOOL isAdmin)
{
	if (!GetDocument()->m_cDocFile.GetFilePathClass().IsValidPath()) {
		ErrorBeep();
		return;
	}

	std::wstring strFolder(GetDocument()->m_cDocFile.GetFilePathClass().GetDirPath());

	/*
		PowerShell でコマンドレットを実行するために -Command を使用する
		Set-Location -Path 'ディレクトリ' で指定したディレクトリに移動する
		-Command を使用する際は -NoExit を指定して PowerShell が終了しないようにする
		(-NoExit がない場合は -Command で指定したコマンドレットが終了すると PowerShellも終了する)
	*/

	// PowerShell の単一引用符文字列内でシングルクォートを '' にエスケープする (OS コマンドインジェクション対策)
	std::wstring strFolderEscaped = strFolder;
	for (size_t pos = 0; (pos = strFolderEscaped.find(L'\'', pos)) != std::wstring::npos; pos += 2)
	{
		strFolderEscaped.replace(pos, 1, L"''");
	}

	CNativeW cmdExeParam;
	cmdExeParam.AppendStringF(L"-NoExit -Command \"Set-Location -Path '%s'\"", strFolderEscaped.c_str());
	LPCWSTR pszcmdExeParam = cmdExeParam.GetStringPtr();

	LPCWSTR pVerb = L"open";
	if (isAdmin)
	{
		pVerb = L"runas";
	}

#ifndef _WIN64
	/*
		64bit OS で 32bit アプリから PowerShell を起動する場合
		通常は 32bit 版の PowerShell が開かれる。

		Wow64 の FileSystem Redirection を一時的にオフにすることにより 64bit 版の
		PowerShell を起動する
	*/
	CDisableWow64FsRedirect wow64Redirect(TRUE);
#endif
	auto hInstance = ::ShellExecuteW(nullptr, pVerb, L"powershell.exe", pszcmdExeParam, strFolder.c_str(), SW_SHOWNORMAL);
	// If the function succeeds, it returns a value greater than 32. 
	if (hInstance <= (decltype(hInstance))32) {
		ErrorBeep();
		return;
	}
}

/* 編集の全終了 */	// 2007.02.13 ryoji 追加
void CViewCommander::Command_EXITALLEDITORS( void )
{
	CControlTray::CloseAllEditor( TRUE, GetMainWindow(), TRUE, 0 );
	return;
}

/* サクラエディタの全終了 */	//Dec. 27, 2000 JEPRO 追加
void CViewCommander::Command_EXITALL( void )
{
	CControlTray::TerminateApplication( GetMainWindow() );	// 2006.12.25 ryoji 引数追加
	return;
}

/*!	@brief 編集中の内容を別名保存

	主に編集中の一時ファイル出力などの目的に使用する．
	現在開いているファイル(m_szFilePath)には影響しない．

	@retval	TRUE 正常終了
	@retval	FALSE ファイル作成に失敗

	@author	maru
	@date	2006.12.10 maru 新規作成
*/
BOOL CViewCommander::Command_PUTFILE(
	LPCWSTR		filename,	//!< [in] filename 出力ファイル名
	ECodeType	nCharCode,	//!< [in] nCharCode 文字コード指定
							//!<  @li CODE_xxxxxxxxxx:各種文字コード
							//!<  @li CODE_AUTODETECT:現在の文字コードを維持
	int			nFlgOpt		//!< [in] nFlgOpt 動作オプション
							//!<  @li 0x01:選択範囲を出力 (非選択状態でも空ファイルを出力する)
)
{
	BOOL		bResult = TRUE;
	ECodeType	nSaveCharCode = nCharCode;
	if(filename[0] == L'\0') {
		return FALSE;
	}

	if(nSaveCharCode == CODE_AUTODETECT) nSaveCharCode = GetDocument()->GetDocumentEncoding();

	//	2007.09.08 genta CEditDoc::FileWrite()にならって砂時計カーソル
	CWaitCursor cWaitCursor( m_pCommanderView->GetHwnd() );

	std::unique_ptr<CCodeBase> pcSaveCode( CCodeFactory::CreateCodeBase(nSaveCharCode,0) );

	bool bBom = false;
	if (CCodeTypeName(nSaveCharCode).UseBom()) {
		bBom = GetDocument()->GetDocumentBomExist();
	}

	if(nFlgOpt & 0x01)
	{	/* 選択範囲を出力 */
		try
		{
			// 選択範囲の取得 -> cMem
			CNativeW cMem;
			m_pCommanderView->GetSelectedDataSimple(cMem);

			// BOM追加
			CNativeW cMem2;
			const CNativeW* pConvBuffer;
			if( bBom ){
				CNativeW cmemBom;
				std::unique_ptr<CCodeBase> pcUtf16( CCodeFactory::CreateCodeBase(CODE_UNICODE,0) );
				pcUtf16->GetBom(cmemBom._GetMemory());
				cMem2.AppendNativeData(cmemBom);
				cMem2.AppendNativeData(cMem);
				cMem.Clear();
				pConvBuffer = &cMem2;
			}else{
				pConvBuffer = &cMem;
			}

			// 書き込み時のコード変換 -> cDst
			CMemory cDst;
			pcSaveCode->UnicodeToCode(*pConvBuffer, &cDst);

			//書込
			if( 0 < cDst.GetRawLength() ){
				CBinaryOutputStream out(filename, true);
				out.Write(cDst.GetRawPtr(), cDst.GetRawLength());
			}
		}
		catch(const CError_FileOpen&)
		{
			WarningMessage(
				nullptr,
				LS(STR_SAVEAGENT_OTHER_APP),
				filename
			);
			bResult = FALSE;
		}
		catch(const CError_FileWrite&)
		{
			WarningMessage(
				nullptr,
				LS(STR_ERR_DLGEDITVWCMDNW11)
			);
			bResult = FALSE;
		}
	}
	else {	/* ファイル全体を出力 */
		HWND		hwndProgress;
		CEditWnd*	pCEditWnd = GetEditWindow();

		if( nullptr != pCEditWnd ){
			hwndProgress = pCEditWnd->m_cStatusBar.GetProgressHwnd();
		}else{
			hwndProgress = nullptr;
		}
		if( nullptr != hwndProgress ){
			::ShowWindow( hwndProgress, SW_SHOW );
		}

		// 一時ファイル出力
		EConvertResult eRet = CWriteManager().WriteFile_From_CDocLineMgr(
			GetDocument()->m_cDocLineMgr,
			SSaveInfo(
				filename,
				nSaveCharCode,
				CEol(EEolType::none),
				bBom
			)
		);
		bResult = (eRet != RESULT_FAILURE);

		if(hwndProgress) ::ShowWindow( hwndProgress, SW_HIDE );
	}
	return bResult;
}

/*!	@brief カーソル位置にファイルを挿入

	現在のカーソル位置に指定のファイルを読み込む．

	@param[in] filename 入力ファイル名
	@param[in] nCharCode 文字コード指定
		@li	CODE_xxxxxxxxxx:各種文字コード
		@li	CODE_AUTODETECT:前回文字コードもしくは自動判別の結果による
	@param[in] nFlgOpt 動作オプション（現在は未定義．0を指定のこと）

	@retval	TRUE 正常終了
	@retval	FALSE ファイルオープンに失敗

	@author	maru
	@date	2006.12.10 maru 新規作成
*/
BOOL CViewCommander::Command_INSFILE( LPCWSTR filename, ECodeType nCharCode, [[maybe_unused]] int nFlgOpt )
{
	CFileLoad	cfl(m_pCommanderView->m_pTypeData->m_encoding);
	CEol cEol;
	int			nLineNum = 0;

	CDlgCancel*	pcDlgCancel = nullptr;
	HWND		hwndCancel = nullptr;
	HWND		hwndProgress = nullptr;
	int			nOldPercent = -1;
	BOOL		bResult = TRUE;

	if(filename[0] == L'\0') {
		return FALSE;
	}

	//	2007.09.08 genta CEditDoc::FileLoad()にならって砂時計カーソル
	CWaitCursor cWaitCursor( m_pCommanderView->GetHwnd() );

	// 範囲選択中なら挿入後も選択状態にするため	/* 2007.04.29 maru */
	BOOL	bBeforeTextSelected = m_pCommanderView->GetSelectionInfo().IsTextSelected();
	CLayoutPoint ptFrom;
	if (bBeforeTextSelected){
		ptFrom = m_pCommanderView->GetSelectionInfo().m_sSelect.GetFrom();
	}

	ECodeType	nSaveCharCode = nCharCode;
	if(nSaveCharCode == CODE_AUTODETECT) {
		EditInfo    fi;
		const CMRUFile  cMRU;
		if ( cMRU.GetEditInfo( filename, &fi ) ){
				nSaveCharCode = fi.m_nCharCode;
		} else {
			nSaveCharCode = GetDocument()->GetDocumentEncoding();
		}
	}

	/* ここまできて文字コードが決定しないならどこかおかしい */
	if( !IsValidCodeOrCPType(nSaveCharCode) ) nSaveCharCode = CODE_SJIS;

	try{
		bool bBigFile;
#ifdef _WIN64
		bBigFile = true;
#else
		bBigFile = false;
#endif
		// ファイルを開く
		cfl.FileOpen( filename, bBigFile, nSaveCharCode, 0 );

		/* ファイルサイズが65KBを越えたら進捗ダイアログ表示 */
		if ( 0x10000 < cfl.GetFileSize() ) {
			pcDlgCancel = new CDlgCancel;
			if( nullptr != ( hwndCancel = pcDlgCancel->DoModeless( ::GetModuleHandle( nullptr ), nullptr, IDD_OPERATIONRUNNING ) ) ){
				hwndProgress = ::GetDlgItem( hwndCancel, IDC_PROGRESS );
				ApiWrap::Progress_SetRange( hwndProgress, 0, 101 );
				ApiWrap::Progress_SetPos( hwndProgress, 0);
			}
		}

		// ReadLineはファイルから 文字コード変換された1行を読み出します
		// エラー時はthrow CError_FileRead を投げます
		CNativeW cBuf;
		while( RESULT_FAILURE != cfl.ReadLine( &cBuf, &cEol ) ){

			const wchar_t*	pLine = cBuf.GetStringPtr();
			int			nLineLen = cBuf.GetStringLength();

			++nLineNum;
			Command_INSTEXT( false, pLine, CLogicInt(nLineLen), true);

			/* 進捗ダイアログ有無 */
			if( nullptr == pcDlgCancel ){
				continue;
			}
			/* 処理中のユーザー操作を可能にする */
			if( !::BlockingHook( pcDlgCancel->GetHwnd() ) ){
				break;
			}
			/* 中断ボタン押下チェック */
			if( pcDlgCancel->IsCanceled() ){
				break;
			}
			if( 0 == ( nLineNum & 0xFF ) ){
				if( nOldPercent != cfl.GetPercent() ){
					ApiWrap::Progress_SetPos( hwndProgress, cfl.GetPercent() + 1 );
					ApiWrap::Progress_SetPos( hwndProgress, cfl.GetPercent() );
					nOldPercent = cfl.GetPercent();
				}
				m_pCommanderView->Redraw();
			}
		}
		// ファイルを明示的に閉じるが、ここで閉じないときはデストラクタで閉じている
		cfl.FileClose();
	} // try
	catch( const CError_FileOpen& ){
		WarningMessage( nullptr, LS(STR_GREP_ERR_FILEOPEN), filename );
		bResult = FALSE;
	}
	catch( const CError_FileRead& ){
		WarningMessage( nullptr, LS(STR_ERR_DLGEDITVWCMDNW12) );
		bResult = FALSE;
	} // 例外処理終わり

	delete pcDlgCancel;

	if (bBeforeTextSelected){	// 挿入された部分を選択状態に
		m_pCommanderView->GetSelectionInfo().SetSelectArea(
			CLayoutRange(
				ptFrom,
				GetCaret().GetCaretLayoutPos()
				/*
				m_nCaretPosY, m_nCaretPosX
				*/
			)
		);
		m_pCommanderView->GetSelectionInfo().DrawSelectArea();
	}
	m_pCommanderView->Redraw();
	return bResult;
}

/*! クイック退避

	現在の文書の写しを、名前も保存場所も聞かずに退避フォルダーへ書き出す。
	無題の文書は Ctrl+S だけで退避先に保存されるので、こちらは
	「もう名前が付いている文書を、退避フォルダーへ複製したい」ときに使う。
	現在の文書のファイル名・パスは変えない。

	@retval true  書き出せた
	@retval false 書き出せなかった（理由はステータスバーに出す）
	@date 2026/08/21 【自前改造】新規作成
*/
bool CViewCommander::Command_QUICK_STASH( void )
{
	const std::wstring strPath = MakeQuickStashPath( GetDocument()->m_cDocLineMgr );
	if( strPath.empty() ){
		m_pCommanderView->SendStatusMessage( L"退避フォルダーを用意できませんでした" );
		return false;
	}

	// 本文を UTF-8 で書き出す（現在の文書のパス・文字コードには一切触らない）
	try{
		CTextOutputStream out( strPath.c_str(), CODE_UTF8, true, true );
		if( !out ){
			m_pCommanderView->SendStatusMessage( L"退避に失敗しました" );
			return false;
		}
		for( const CDocLine* pcDocLine = GetDocument()->m_cDocLineMgr.GetDocLineTop();
			 nullptr != pcDocLine;
			 pcDocLine = pcDocLine->GetNextLine() )
		{
			const int nLen = (Int)pcDocLine->GetLengthWithoutEOL();
			if( 0 < nLen ){
				out.WriteString( pcDocLine->GetPtr(), nLen );
			}
			if( 0 < pcDocLine->GetEol().GetLen() ){
				out.WriteString( L"\n" );
			}
		}
		out.Close();
	}
	catch( ... ){
		m_pCommanderView->SendStatusMessage( L"退避に失敗しました" );
		return false;
	}

	std::wstring strMsg = L"退避しました: ";
	strMsg += strPath;
	m_pCommanderView->SendStatusMessage( strMsg.c_str() );
	return true;
}

/*! 退避を開く

	退避フォルダーを開いた状態で「ファイルを開く」ダイアログを出す。
	ファイル名が「日時＋見出し」なので、そのまま見分けて開ける（削除も Delete キーでできる）。

	@date 2026/08/21 【自前改造】新規作成
*/
void CViewCommander::Command_QUICK_STASH_OPEN( void )
{
	const std::wstring strDir = GetQuickStashDir();
	if( strDir.empty() ){
		m_pCommanderView->SendStatusMessage( L"退避フォルダーが見つかりません" );
		return;
	}
	// 末尾に \ を付けてフォルダーとして渡す
	const std::wstring strDirArg = strDir + L"\\";
	Command_FILEOPEN( nullptr, CODE_AUTODETECT, false, strDirArg.c_str() );
}

/*! 今すぐ最新版を確認

	共通仕様（C:\dev\アプリ共通仕様.md 第3部）の「設定画面に置く更新の行」に相当する。
	版・配布されている版・最終確認日時をまとめて見せ、新しければその場で適用できる。

	@date 2026/08/21 【自前改造】新規作成
*/
void CViewCommander::Command_CHECK_UPDATE( void )
{
	const HWND hwndOwner = CEditWnd::getInstance()->GetHwnd();

	const SUpdateCheckResult res = CheckUpdate();

	std::wstring strMsg;
	strMsg += L"現在の版\t: ";
	strMsg += res.m_cRunning.ToString();
	strMsg += L"\n";

	if( !res.m_bDistFound ){
		strMsg += L"配布されている版\t: 確認できません\n";
		strMsg += L"\n";
		strMsg += L"配布フォルダーが見つかりませんでした。\n";
		strMsg += L"Google ドライブ（H:）が同期されているか確認してください。";
		::MessageBox( hwndOwner, strMsg.c_str(), L"更新の確認", MB_OK | MB_ICONINFORMATION );
		return;
	}

	strMsg += L"配布されている版\t: ";
	strMsg += res.m_cDist.ToString();
	strMsg += L"\n";
	strMsg += L"最終確認\t: ";
	strMsg += GetLastUpdateCheckTime();
	strMsg += L"\n\n";

	if( !res.m_bAvailable ){
		strMsg += L"最新です。";
		::MessageBox( hwndOwner, strMsg.c_str(), L"更新の確認", MB_OK | MB_ICONINFORMATION );
		return;
	}

	strMsg += L"新しい版があります。\n";
	strMsg += L"今すぐ再起動して適用しますか？\n";
	strMsg += L"（編集中のファイルは先に保存してください）";
	if( IDYES != ::MessageBox( hwndOwner, strMsg.c_str(), L"更新の確認", MB_YESNO | MB_ICONQUESTION ) ){
		return;
	}

	if( !StartUpdate() ){
		::MessageBox( hwndOwner, L"更新を開始できませんでした。", L"更新の確認", MB_OK | MB_ICONWARNING );
		return;
	}
	// 更新スクリプトが全プロセスの終了を待っているので、速やかに全部閉じる
	HandleCommand( F_EXITALL, true, 0, 0, 0, 0 );
}
