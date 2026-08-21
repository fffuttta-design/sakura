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
	WCHAR szBuf[32];
	if( PLUS_VERSION_BASE < m_nBuild ){
		::auto_sprintf_s( szBuf, _countof(szBuf), L"v%d", m_nBuild - PLUS_VERSION_BASE );
	}else{
		// 起点より前（＝改造前のもの）は、そのままビルド番号を見せる
		::auto_sprintf_s( szBuf, _countof(szBuf), L"v%d", m_nBuild );
	}
	return szBuf;
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

bool StartUpdate()
{
	const std::wstring strDist = FindDistDir();
	if( strDist.empty() ){
		return false;
	}
	const std::wstring strSelf = GetSelfDir();

	// 更新スクリプトを一時フォルダーに書き出す
	WCHAR szTemp[_MAX_PATH];
	szTemp[0] = L'\0';
	if( 0 == ::GetTempPath( _countof(szTemp), szTemp ) ){
		return false;
	}
	std::wstring strBat = szTemp;
	strBat += L"SakuraEditorPlus-update.bat";

	// 動いている exe は上書きできないので、コピーできるまで1秒おきに再試行する。
	// 全プロセスが終われば通る。60回（約1分）で諦める。
	std::wstring strScript;
	strScript += L"@echo off\r\n";
	strScript += L"chcp 65001 > nul\r\n";
	strScript += L"setlocal\r\n";
	strScript += L"set SRC=" + strDist + L"\r\n";
	strScript += L"set DST=" + strSelf + L"\r\n";
	strScript += L"set N=0\r\n";
	strScript += L":retry\r\n";
	strScript += L"set /a N+=1\r\n";
	strScript += L"if %N% GTR 60 goto giveup\r\n";
	// /XF で設定ファイルを除外（引き継いだ設定を消さない）
	strScript += L"robocopy \"%SRC%\" \"%DST%\" /E /R:0 /W:0 /NJH /NJS /NP /NFL /NDL "
	             L"/XF sakura.ini sakura.ini.* update-check.txt > nul\r\n";
	strScript += L"if errorlevel 8 (\r\n";
	strScript += L"  ping -n 2 127.0.0.1 > nul\r\n";
	strScript += L"  goto retry\r\n";
	strScript += L")\r\n";
	strScript += L"start \"\" \"%DST%\\sakura.exe\"\r\n";
	strScript += L"goto done\r\n";
	strScript += L":giveup\r\n";
	strScript += L"start \"\" \"%DST%\\sakura.exe\"\r\n";
	strScript += L":done\r\n";
	strScript += L"del \"%~f0\" > nul 2>&1\r\n";

	// バッチは UTF-8(BOM無し) ＋ chcp 65001 で日本語パスを通す
	const int nUtf8Len = ::WideCharToMultiByte( CP_UTF8, 0, strScript.c_str(), (int)strScript.length(), nullptr, 0, nullptr, nullptr );
	if( nUtf8Len <= 0 ){
		return false;
	}
	std::vector<char> vUtf8( nUtf8Len );
	::WideCharToMultiByte( CP_UTF8, 0, strScript.c_str(), (int)strScript.length(), vUtf8.data(), nUtf8Len, nullptr, nullptr );

	const HANDLE hFile = ::CreateFile( strBat.c_str(), GENERIC_WRITE, 0, nullptr,
		CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr );
	if( INVALID_HANDLE_VALUE == hFile ){
		return false;
	}
	DWORD dwWritten = 0;
	const BOOL bWrote = ::WriteFile( hFile, vUtf8.data(), (DWORD)nUtf8Len, &dwWritten, nullptr );
	::CloseHandle( hFile );
	if( !bWrote ){
		return false;
	}

	// 画面を出さずに実行する
	std::wstring strCmd = L"cmd.exe /c \"" + strBat + L"\"";
	std::vector<WCHAR> vCmd( strCmd.begin(), strCmd.end() );
	vCmd.push_back( L'\0' );

	STARTUPINFO si = { sizeof(si) };
	si.dwFlags     = STARTF_USESHOWWINDOW;
	si.wShowWindow = SW_HIDE;
	PROCESS_INFORMATION pi = {};
	if( !::CreateProcess( nullptr, vCmd.data(), nullptr, nullptr, FALSE,
			CREATE_NO_WINDOW, nullptr, szTemp, &si, &pi ) ){
		return false;
	}
	::CloseHandle( pi.hThread );
	::CloseHandle( pi.hProcess );
	return true;
}
