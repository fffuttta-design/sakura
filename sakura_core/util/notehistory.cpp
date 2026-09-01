/*! @file */
/*
	【自前改造】メモの変更履歴

	SPDX-License-Identifier: Zlib
*/
#include "StdAfx.h"
#include "util/notehistory.h"

#include "util/file.h"
#include "window/CNoteBar.h"

#include <algorithm>

namespace {

//! フルパスをフォルダーと名前に割る
void SplitPathName( const std::wstring& strPath, std::wstring* pDir, std::wstring* pName )
{
	const size_t nPos = strPath.find_last_of( L"\\/" );
	if( std::wstring::npos == nPos ){
		if( pDir ) pDir->clear();
		if( pName ) *pName = strPath;
		return;
	}
	if( pDir )  *pDir  = strPath.substr( 0, nPos );
	if( pName ) *pName = strPath.substr( nPos + 1 );
}

//! ファイルの中身をまるごと読む（大きすぎるものは諦める）
bool ReadWholeFile( const std::wstring& strPath, std::vector<BYTE>* pOut )
{
	pOut->clear();
	const HANDLE hFile = ::CreateFile( strPath.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr );
	if( INVALID_HANDLE_VALUE == hFile ){
		return false;
	}
	LARGE_INTEGER liSize;
	bool bOk = false;
	if( ::GetFileSizeEx( hFile, &liSize ) && liSize.QuadPart <= 8 * 1024 * 1024 ){
		if( 0 < liSize.QuadPart ){
			pOut->resize( (size_t)liSize.QuadPart );
			DWORD dwRead = 0;
			bOk = ( FALSE != ::ReadFile( hFile, &(*pOut)[0], (DWORD)pOut->size(), &dwRead, nullptr ) )
			   && ( dwRead == pOut->size() );
		}else{
			bOk = true;		// 空のファイル
		}
	}
	::CloseHandle( hFile );
	if( !bOk ){
		pOut->clear();
	}
	return bOk;
}

//! 履歴に付ける名前（そのファイルの更新日時から作る）
std::wstring MakeStampName( const std::wstring& strPath )
{
	WIN32_FILE_ATTRIBUTE_DATA fad;
	SYSTEMTIME stLocal;
	::ZeroMemory( &stLocal, sizeof(stLocal) );
	if( ::GetFileAttributesEx( strPath.c_str(), GetFileExInfoStandard, &fad ) ){
		SYSTEMTIME stUtc;
		if( !::FileTimeToSystemTime( &fad.ftLastWriteTime, &stUtc )
		 || !::SystemTimeToTzSpecificLocalTime( nullptr, &stUtc, &stLocal ) ){
			::GetLocalTime( &stLocal );
		}
	}else{
		::GetLocalTime( &stLocal );
	}
	WCHAR szBuf[64];
	::wsprintf( szBuf, L"%04d-%02d-%02d_%02d%02d%02d",
		stLocal.wYear, stLocal.wMonth, stLocal.wDay,
		stLocal.wHour, stLocal.wMinute, stLocal.wSecond );
	return std::wstring( szBuf ) + L".txt";
}

} // namespace

bool IsNoteFile( const std::wstring& strPath )
{
	if( strPath.empty() ){
		return false;
	}
	const std::wstring strNoteDir = CNoteBar::GetNoteFolder();
	if( strNoteDir.empty() ){
		return false;
	}
	std::wstring strDir;
	SplitPathName( strPath, &strDir, nullptr );
	// 末尾の \ の有無で食い違わないようにそろえてから比べる
	while( !strDir.empty() && L'\\' == strDir.back() ){ strDir.pop_back(); }
	std::wstring strNote = strNoteDir;
	while( !strNote.empty() && L'\\' == strNote.back() ){ strNote.pop_back(); }
	return ( 0 == _wcsicmp( strDir.c_str(), strNote.c_str() ) );
}

std::wstring GetNoteHistoryDir( const std::wstring& strNotePath )
{
	if( !IsNoteFile( strNotePath ) ){
		return std::wstring();
	}
	std::wstring strDir, strName;
	SplitPathName( strNotePath, &strDir, &strName );
	std::wstring strHist = strDir;
	strHist += L"\\";
	strHist += NOTE_HISTORY_DIR_NAME;
	strHist += L"\\";
	strHist += strName;
	return strHist;
}

void SaveNoteHistory( const std::wstring& strNotePath )
{
	const std::wstring strHistDir = GetNoteHistoryDir( strNotePath );
	if( strHistDir.empty() || !fexist( strNotePath.c_str() ) ){
		return;		// ノートでない／まだ実体が無い（新規保存）ので残すものが無い
	}

	std::vector<BYTE> vNow;
	if( !ReadWholeFile( strNotePath, &vNow ) ){
		return;		// 読めないものは残さない（保存自体は止めない）
	}

	// 直前の履歴と同じ中身なら残さない（自動保存などで同じものが積み上がるのを防ぐ）
	const std::vector<std::wstring> vOld = ListNoteHistory( strNotePath );
	if( !vOld.empty() ){
		std::vector<BYTE> vPrev;
		if( ReadWholeFile( vOld[0], &vPrev ) && vPrev == vNow ){
			return;
		}
	}

	// `.履歴` → `.履歴\<メモ名>` の順に掘る
	std::wstring strParent;
	SplitPathName( strHistDir, &strParent, nullptr );
	if( !IsDirectory( strParent.c_str() ) && !::CreateDirectory( strParent.c_str(), nullptr ) ){
		return;
	}
	// メモではないので隠す（一覧はフォルダーを出さないが、エクスプローラーでも邪魔しない）
	::SetFileAttributes( strParent.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_DIRECTORY );
	if( !IsDirectory( strHistDir.c_str() ) && !::CreateDirectory( strHistDir.c_str(), nullptr ) ){
		return;
	}

	std::wstring strDest = strHistDir;
	strDest += L"\\";
	strDest += MakeStampName( strNotePath );
	if( fexist( strDest.c_str() ) ){
		return;		// 同じ更新日時＝同じ版。もう残してある
	}
	::CopyFile( strNotePath.c_str(), strDest.c_str(), TRUE );

	// 増えすぎたら古いものから捨てる（名前が日時なので、名前の昇順＝古い順）
	std::vector<std::wstring> vAll = ListNoteHistory( strNotePath );
	for( size_t i = (size_t)NOTE_HISTORY_MAX; i < vAll.size(); ++i ){
		::DeleteFile( vAll[i].c_str() );
	}
}

std::vector<std::wstring> ListNoteHistory( const std::wstring& strNotePath )
{
	std::vector<std::wstring> vRet;
	const std::wstring strHistDir = GetNoteHistoryDir( strNotePath );
	if( strHistDir.empty() ){
		return vRet;
	}
	const std::wstring strFind = strHistDir + L"\\*.txt";
	WIN32_FIND_DATA fd;
	const HANDLE hFind = ::FindFirstFile( strFind.c_str(), &fd );
	if( INVALID_HANDLE_VALUE == hFind ){
		return vRet;
	}
	do {
		if( 0 != (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ){
			continue;
		}
		vRet.push_back( strHistDir + L"\\" + fd.cFileName );
	} while( ::FindNextFile( hFind, &fd ) );
	::FindClose( hFind );

	// 名前が「YYYY-MM-DD_HHMMSS」なので、名前の降順＝新しい順
	std::sort( vRet.begin(), vRet.end(), []( const std::wstring& a, const std::wstring& b ){
		return 0 < _wcsicmp( a.c_str(), b.c_str() );
	} );
	return vRet;
}

void RenameNoteHistory( const std::wstring& strOldPath, const std::wstring& strNewPath )
{
	const std::wstring strOldDir = GetNoteHistoryDir( strOldPath );
	const std::wstring strNewDir = GetNoteHistoryDir( strNewPath );
	if( strOldDir.empty() || strNewDir.empty() ){
		return;
	}
	if( !IsDirectory( strOldDir.c_str() ) || IsDirectory( strNewDir.c_str() ) ){
		return;		// 元が無い／行き先が既にある（混ぜない）
	}
	::MoveFile( strOldDir.c_str(), strNewDir.c_str() );
}
