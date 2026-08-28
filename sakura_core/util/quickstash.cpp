/*! @file
	@brief 【自前改造】クイック退避（ドキュメント）の共通処理

	SPDX-License-Identifier: Zlib
*/
#include "StdAfx.h"
#include "util/quickstash.h"

#include "util/file.h"

#include <algorithm>

namespace {
//! 候補1: Google ドライブ（H: = contact@run-strategy.jp を優先。次に G: 個人、I: 家族）
const WCHAR* const ppszDriveCandidates[] = { L"H:", L"G:", L"I:" };
const WCHAR* const ppszSubDirs[] = { L"マイドライブ", L"ツール開発", L"SakuraEditorPlus", L"退避" };
}	// namespace

/*! Google ドライブがまだ現れていない（接続待ち）か

	Windows にサインインした直後は、Google ドライブ（デスクトップ版）が
	ドライブを生やすまでに少し時間がかかる。その間に退避フォルダーを探すと
	「無い」ことになってしまう。
	この端末に Google ドライブが入っているのに候補ドライブが1つも見えない
	＝「まだ来ていないだけ」と見なして、待つ側に倒す。
*/
bool IsQuickStashDriveWaiting()
{
	for( const WCHAR* pszDrive : ppszDriveCandidates ){
		std::wstring strPath = pszDrive;
		strPath += L"\\";
		strPath += ppszSubDirs[0];
		if( IsDirectory( strPath.c_str() ) ){
			return false;	// もう来ている
		}
	}
	// そもそも Google ドライブが入っていない端末なら、いくら待っても来ない
	WCHAR szLocal[_MAX_PATH];
	if( 0 == ::GetEnvironmentVariable( L"LOCALAPPDATA", szLocal, _countof(szLocal) ) ){
		return false;
	}
	std::wstring strDriveFs = szLocal;
	strDriveFs += L"\\Google\\DriveFS";
	return IsDirectory( strDriveFs.c_str() );
}

std::wstring GetQuickStashDir()
{
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

	// 🔥 接続待ちのときはローカルへ逃がさない。
	//    ここで逃がすと、あとからドライブが来たときに一覧へ出てこないメモが生まれる
	//    （2026-08-28、空の「%USERPROFILE%\SakuraEditorPlus退避」ができていて発覚）
	if( IsQuickStashDriveWaiting() ){
		return std::wstring();
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

bool ParseStashNameTime( LPCWSTR pszFileName, FILETIME* pftOut )
{
	if( nullptr == pszFileName ){
		return false;
	}
	// 「YYYY-MM-DD_hhmm」の形か
	const size_t nLen = wcslen( pszFileName );
	if( nLen < 15 ){
		return false;
	}
	auto isDigits = []( LPCWSTR p, size_t nPos, size_t nCount ){
		for( size_t i = 0; i < nCount; ++i ){
			if( p[nPos + i] < L'0' || L'9' < p[nPos + i] ) return false;
		}
		return true;
	};
	if( !isDigits( pszFileName, 0, 4 ) || L'-' != pszFileName[4]
	 || !isDigits( pszFileName, 5, 2 ) || L'-' != pszFileName[7]
	 || !isDigits( pszFileName, 8, 2 ) || L'_' != pszFileName[10]
	 || !isDigits( pszFileName, 11, 4 ) ){
		return false;
	}
	auto toInt = []( LPCWSTR p, size_t nPos, size_t nCount ){
		int n = 0;
		for( size_t i = 0; i < nCount; ++i ){
			n = n * 10 + (int)( p[nPos + i] - L'0' );
		}
		return n;
	};
	SYSTEMTIME st;
	::ZeroMemory( &st, sizeof(st) );
	st.wYear   = (WORD)toInt( pszFileName, 0, 4 );
	st.wMonth  = (WORD)toInt( pszFileName, 5, 2 );
	st.wDay    = (WORD)toInt( pszFileName, 8, 2 );
	st.wHour   = (WORD)toInt( pszFileName, 11, 2 );
	st.wMinute = (WORD)toInt( pszFileName, 13, 2 );
	if( st.wMonth < 1 || 12 < st.wMonth || st.wDay < 1 || 31 < st.wDay
	 || 23 < st.wHour || 59 < st.wMinute ){
		return false;
	}
	// 名前に入っているのは地方時なので、UTC に直してから比べる
	// （更新日時の FILETIME は UTC。直さないと時差のぶんだけ並び順が狂う）
	SYSTEMTIME stUtc;
	if( !::TzSpecificLocalTimeToSystemTime( nullptr, &st, &stUtc ) ){
		return false;
	}
	return FALSE != ::SystemTimeToFileTime( &stUtc, pftOut );
}

std::vector<std::wstring> GetQuickStashFiles( int nMax )
{
	return GetStashFilesInDir( GetQuickStashDir(), nMax );
}

std::vector<std::wstring> GetStashFilesInDir( const std::wstring& strDir, int nMax )
{
	std::vector<std::wstring> vRet;
	if( nMax <= 0 || strDir.empty() ){
		return vRet;
	}

	// 並べ替えのために日時つきで集める。
	// 名前が「2026-08-25_1218 ...」で始まっていれば、そこの日時を並び順に使う
	// （一覧に出す日時と並び順を一致させるため。一致していないと「新しい順のはずなのに
	//   古い日付が上にある」という見え方になる）
	struct SEntry {
		FILETIME		ftSort;		//!< 並べ替えに使う日時
		FILETIME		ftWrite;	//!< 更新日時（同じ分に何個もあるときの決着用）
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
		if( !ParseStashNameTime( fd.cFileName, &e.ftSort ) ){
			e.ftSort = fd.ftLastWriteTime;
		}
		e.strPath  = strDir;
		e.strPath += L"\\";
		e.strPath += fd.cFileName;
		vEntries.push_back( e );
	} while( ::FindNextFile( hFind, &fd ) );
	::FindClose( hFind );

	// 新しい順（あとで書いたものほど上に出したい）
	std::sort( vEntries.begin(), vEntries.end(), []( const SEntry& a, const SEntry& b ){
		const int nCmp = ::CompareFileTime( &a.ftSort, &b.ftSort );
		if( 0 != nCmp ){
			return 0 < nCmp;
		}
		return 0 < ::CompareFileTime( &a.ftWrite, &b.ftWrite );
	} );

	const int nCount = std::min<int>( nMax, (int)vEntries.size() );
	vRet.reserve( nCount );
	for( int i = 0; i < nCount; ++i ){
		vRet.push_back( vEntries[i].strPath );
	}
	return vRet;
}
