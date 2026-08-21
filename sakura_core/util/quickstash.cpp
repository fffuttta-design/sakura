/*! @file
	@brief 【自前改造】クイック退避（ドキュメント）の共通処理

	SPDX-License-Identifier: Zlib
*/
#include "StdAfx.h"
#include "util/quickstash.h"

#include "util/file.h"

#include <algorithm>

std::wstring GetQuickStashDir()
{
	// 候補1: Google ドライブ（H: = contact@run-strategy.jp を優先。次に G: 個人、I: 家族）
	static const WCHAR* const ppszDriveCandidates[] = { L"H:", L"G:", L"I:" };
	static const WCHAR* const ppszSubDirs[] = { L"マイドライブ", L"ツール開発", L"SakuraEditorPlus", L"退避" };

	for( const WCHAR* pszDrive : ppszDriveCandidates ){
		std::wstring strPath = pszDrive;
		strPath += L"\\";
		strPath += ppszSubDirs[0];
		if( !IsDirectory( strPath.c_str() ) ){
			continue;	// このドライブはマウントされていない
		}
		// マイドライブの下を順に掘る（無ければ作る）
		bool bOk = true;
		for( size_t i = 1; i < _countof(ppszSubDirs); ++i ){
			strPath += L"\\";
			strPath += ppszSubDirs[i];
			if( !IsDirectory( strPath.c_str() ) && !::CreateDirectory( strPath.c_str(), nullptr ) ){
				bOk = false;
				break;
			}
		}
		if( bOk ){
			return strPath;
		}
	}

	// 候補2: ユーザーフォルダー（Drive が無い端末でも動くように）
	WCHAR szProfile[_MAX_PATH];
	szProfile[0] = L'\0';
	if( 0 < ::GetEnvironmentVariable( L"USERPROFILE", szProfile, _countof(szProfile) ) ){
		std::wstring strPath = szProfile;
		strPath += L"\\SakuraEditorPlus退避";
		if( IsDirectory( strPath.c_str() ) || ::CreateDirectory( strPath.c_str(), nullptr ) ){
			return strPath;
		}
	}

	return std::wstring();
}

std::vector<std::wstring> GetQuickStashFiles( int nMax )
{
	std::vector<std::wstring> vRet;
	if( nMax <= 0 ){
		return vRet;
	}

	const std::wstring strDir = GetQuickStashDir();
	if( strDir.empty() ){
		return vRet;
	}

	// 更新日時で並べ替えるので、いったん日時つきで集める
	struct SEntry {
		FILETIME		ftWrite;
		std::wstring	strPath;
	};
	std::vector<SEntry> vEntries;

	const std::wstring strFind = strDir + L"\\*.*";
	WIN32_FIND_DATA fd;
	const HANDLE hFind = ::FindFirstFile( strFind.c_str(), &fd );
	if( INVALID_HANDLE_VALUE == hFind ){
		return vRet;
	}
	do {
		if( 0 != (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
			continue;	// フォルダーは出さない
		}
		// Google ドライブが置く desktop.ini など、自分で書いたものでないファイルは出さない
		if( 0 != (fd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) ){
			continue;
		}
		SEntry e;
		e.ftWrite  = fd.ftLastWriteTime;
		e.strPath  = strDir;
		e.strPath += L"\\";
		e.strPath += fd.cFileName;
		vEntries.push_back( e );
	} while( ::FindNextFile( hFind, &fd ) );
	::FindClose( hFind );

	// 新しい順（あとで書いたものほど上に出したい）
	std::sort( vEntries.begin(), vEntries.end(), []( const SEntry& a, const SEntry& b ){
		return 0 < ::CompareFileTime( &a.ftWrite, &b.ftWrite );
	} );

	const int nCount = std::min<int>( nMax, (int)vEntries.size() );
	vRet.reserve( nCount );
	for( int i = 0; i < nCount; ++i ){
		vRet.push_back( vEntries[i].strPath );
	}
	return vRet;
}
