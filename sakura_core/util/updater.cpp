/*! @file
	@brief 【自前改造】SakuraEditorPlus の自動更新／手動更新

	SPDX-License-Identifier: Zlib
*/
#include "StdAfx.h"
#include "util/updater.h"

#include "util/file.h"
#include "util/string_ex.h"

#include <vector>

#pragma comment(lib, "version.lib")

namespace {

//! 配布フォルダーを探す（H: 会社 → G: 個人 → I: 家族 の順）
std::wstring FindDistDir()
{
	static const WCHAR* const ppszDrives[] = { L"H:", L"G:", L"I:" };
	for( const WCHAR* pszDrive : ppszDrives ){
		std::wstring strPath = pszDrive;
		strPath += L"\\マイドライブ\\ツール開発\\SakuraEditorPlus\\配布\\app";
		if( IsDirectory( strPath.c_str() ) ){
			std::wstring strExe = strPath + L"\\sakura.exe";
			if( fexist( strExe.c_str() ) ){
				return strPath;
			}
		}
	}
	return std::wstring();
}

//! 自分が入っているフォルダー
std::wstring GetSelfDir()
{
	WCHAR szPath[_MAX_PATH];
	szPath[0] = L'\0';
	::GetModuleFileName( nullptr, szPath, _countof(szPath) );
	WCHAR szDrive[_MAX_DRIVE];
	WCHAR szDir  [_MAX_DIR];
	_wsplitpath_s( szPath, szDrive, _countof(szDrive), szDir, _countof(szDir), nullptr, 0, nullptr, 0 );
	std::wstring strDir = szDrive;
	strDir += szDir;
	// 末尾の \ を落とす
	while( !strDir.empty() && L'\\' == strDir[strDir.length()-1] ){
		strDir.erase( strDir.length() - 1 );
	}
	return strDir;
}

//! 最終確認日時を書いておくファイル
std::wstring GetStampPath()
{
	return GetSelfDir() + L"\\update-check.txt";
}

} // namespace

std::wstring SAppVersion::ToString() const
{
	WCHAR szBuf[64];
	::auto_sprintf_s( szBuf, _countof(szBuf), L"%d.%d.%d.%d", m_nMajor, m_nMinor, m_nPatch, m_nBuild );
	return szBuf;
}

std::wstring SAppVersion::ToShortString() const
{
	return MakePlusVersionString( m_nBuild );
}

std::wstring GetUpdateDistDir()
{
	return FindDistDir();
}

SAppVersion GetExeVersion( const WCHAR* pszExePath )
{
	SAppVersion ver;
	if( nullptr == pszExePath || L'\0' == pszExePath[0] ){
		return ver;
	}

	DWORD dwDummy = 0;
	const DWORD dwSize = ::GetFileVersionInfoSize( pszExePath, &dwDummy );
	if( 0 == dwSize ){
		return ver;
	}

	std::vector<BYTE> vBuf( dwSize );
	if( !::GetFileVersionInfo( pszExePath, 0, dwSize, vBuf.data() ) ){
		return ver;
	}

	VS_FIXEDFILEINFO* pInfo = nullptr;
	UINT nLen = 0;
	if( !::VerQueryValue( vBuf.data(), L"\\", (LPVOID*)&pInfo, &nLen ) || nullptr == pInfo ){
		return ver;
	}

	ver.m_nMajor = HIWORD( pInfo->dwFileVersionMS );
	ver.m_nMinor = LOWORD( pInfo->dwFileVersionMS );
	ver.m_nPatch = HIWORD( pInfo->dwFileVersionLS );
	ver.m_nBuild = LOWORD( pInfo->dwFileVersionLS );
	return ver;
}

SAppVersion GetRunningVersion()
{
	WCHAR szPath[_MAX_PATH];
	szPath[0] = L'\0';
	::GetModuleFileName( nullptr, szPath, _countof(szPath) );
	return GetExeVersion( szPath );
}

SAppVersion GetDistVersion()
{
	const std::wstring strDir = FindDistDir();
	if( strDir.empty() ){
		return SAppVersion();
	}
	const std::wstring strExe = strDir + L"\\sakura.exe";
	return GetExeVersion( strExe.c_str() );
}

SUpdateCheckResult CheckUpdate()
{
	SUpdateCheckResult res;
	res.m_cRunning = GetRunningVersion();
	res.m_cDist    = GetDistVersion();
	res.m_bDistFound = res.m_cDist.IsValid();
	res.m_bAvailable = res.m_bDistFound && res.m_cRunning.IsValid() && ( res.m_cDist > res.m_cRunning );

	SetLastUpdateCheckTime();
	return res;
}

void SetLastUpdateCheckTime()
{
	SYSTEMTIME st;
	::GetLocalTime( &st );
	WCHAR szBuf[64];
	::auto_sprintf_s( szBuf, _countof(szBuf), L"%04d-%02d-%02d %02d:%02d",
		st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute );

	const std::wstring strPath = GetStampPath();
	const HANDLE hFile = ::CreateFile( strPath.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
	if( INVALID_HANDLE_VALUE == hFile ){
		return;	// 書けなくても動作に影響はない
	}
	DWORD dwWritten = 0;
	::WriteFile( hFile, szBuf, (DWORD)(wcslen(szBuf) * sizeof(WCHAR)), &dwWritten, nullptr );
	::CloseHandle( hFile );
}

std::wstring GetLastUpdateCheckTime()
{
	const std::wstring strPath = GetStampPath();
	const HANDLE hFile = ::CreateFile( strPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	if( INVALID_HANDLE_VALUE == hFile ){
		return std::wstring();
	}
	WCHAR szBuf[64];
	DWORD dwRead = 0;
	const BOOL bOk = ::ReadFile( hFile, szBuf, sizeof(szBuf) - sizeof(WCHAR), &dwRead, nullptr );
	::CloseHandle( hFile );
	if( !bOk || 0 == dwRead ){
		return std::wstring();
	}
	szBuf[dwRead / sizeof(WCHAR)] = L'\0';
	return szBuf;
}

bool StartUpdate( std::wstring* pstrError )
{
	const auto fail = [pstrError]( const wchar_t* pszWhy ) -> bool {
		if( pstrError ){ *pstrError = pszWhy; }
		return false;
	};
	const std::wstring strDist = FindDistDir();
	if( strDist.empty() ){
		return fail( L"配布フォルダー（H:\\マイドライブ\\ツール開発\\SakuraEditorPlus\\配布\\app）が見つかりません。" );
	}
	const std::wstring strSelf = GetSelfDir();

	WCHAR szTemp[_MAX_PATH];
	szTemp[0] = L'\0';
	if( 0 == ::GetTempPath( _countof(szTemp), szTemp ) ){
		return fail( L"一時フォルダーの場所が取れませんでした。" );
	}

	// 更新の実体は同梱の updater.ps1（「更新しています」の窓と進捗バーを出す）。
	// 自分自身も更新対象なので、一時フォルダーへ写してからそちらを動かす。
	// （導入先で直接動かすと、自分を上書きしようとして失敗する）
	const std::wstring strSrcScript = strSelf + L"\\updater.ps1";
	if( !fexist( strSrcScript.c_str() ) ){
		return fail( L"updater.ps1 が入っていません（アプリのフォルダーを確認してください）。" );
	}
	std::wstring strRunScript = szTemp;
	strRunScript += L"SakuraEditorPlus-update.ps1";
	if( !::CopyFile( strSrcScript.c_str(), strRunScript.c_str(), FALSE ) ){
		WCHAR szWhy[256];
		::_snwprintf_s( szWhy, _countof(szWhy), _TRUNCATE,
			L"更新スクリプトを一時フォルダーへ写せませんでした（エラー %u）。", ::GetLastError() );
		return fail( szWhy );
	}

	// 🔥 powershell.exe は**フルパスで**起動する。PATH 頼みだと、環境によっては
	//    CreateProcess が「ファイルが見つかりません」で落ちて、更新が始まらない。
	WCHAR szSys[_MAX_PATH];
	szSys[0] = L'\0';
	if( 0 == ::GetSystemDirectory( szSys, _countof(szSys) ) ){
		return fail( L"Windows のシステムフォルダーの場所が取れませんでした。" );
	}
	std::wstring strPwsh = szSys;
	strPwsh += L"\\WindowsPowerShell\\v1.0\\powershell.exe";
	if( !fexist( strPwsh.c_str() ) ){
		strPwsh = L"powershell.exe";	// 念のため PATH 頼みへ落とす
	}

	// 引数のパスは空白を含むので必ず引用符で囲む
	std::wstring strCmd;
	strCmd += L"\"";
	strCmd += strPwsh;
	strCmd += L"\" -NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File \"";
	strCmd += strRunScript;
	strCmd += L"\" -SrcDir \"";
	strCmd += strDist;
	strCmd += L"\" -DstDir \"";
	strCmd += strSelf;
	strCmd += L"\"";

	std::vector<WCHAR> vCmd( strCmd.begin(), strCmd.end() );
	vCmd.push_back( L'\0' );

	// コンソールは出さない（進捗はスクリプト側の窓で見せる）
	STARTUPINFO si = { sizeof(si) };
	si.dwFlags     = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi = {};
	if( !::CreateProcess( nullptr, vCmd.data(), nullptr, nullptr, FALSE,
			CREATE_NO_WINDOW, nullptr, szTemp, &si, &pi ) ){
		WCHAR szWhy[256];
		::_snwprintf_s( szWhy, _countof(szWhy), _TRUNCATE,
			L"更新スクリプトを起動できませんでした（エラー %u）。", ::GetLastError() );
		return fail( szWhy );
	}
	::CloseHandle( pi.hThread );
	::CloseHandle( pi.hProcess );
	return true;
}
