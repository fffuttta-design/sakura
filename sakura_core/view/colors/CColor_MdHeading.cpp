/*! @file */
/*
	【自前改造】Markdown の見出し（# ## ###）の色分け

	SPDX-License-Identifier: Zlib
*/
#include "StdAfx.h"
#include "view/CEditView.h" // SColorStrategyInfo
#include "CColor_MdHeading.h"
#include "doc/CEditDoc.h"
#include "types/CTypeSupport.h"
#include "util/markdown.h"	// 【自前改造】拡張子の判定

/*! 今開いているのが Markdown かを見ておく

	ファイルを開き直したとき・タイプ別設定を変えたときに呼ばれる（CDocType 経由）。
	∴ ここで拡張子を見ておけば、描画のたびに調べ直さなくてよい。
*/
void CColor_MdHeading::Update(void)
{
	CColorStrategy::Update();

	m_bMarkdown = false;
	const CEditDoc* pcDoc = CEditDoc::GetInstance(0);
	if( nullptr == pcDoc ){
		return;
	}
	const WCHAR* pszPath = pcDoc->m_cDocFile.GetFilePath();
	if( nullptr == pszPath ){
		return;
	}
	m_bMarkdown = IsMarkdownPath( pszPath );
}

namespace {

//! 行頭が見出しなら、段（1〜3）と本文の始まる位置を返す
/*!
	`#` は1〜6個。そのうしろの空白は本文に含めない（記号ごと飲み込むため）。
	🔥 空白は要求しない。本人は `#見出し` と詰めて書く（2026-09-01）。
	   行頭でしか見ないので、文中のハッシュタグを拾う心配は無い。
*/
bool ParseHeading( const CStringRef& cStr, int* pnLevel, int* pnTextStart )
{
	if( !cStr.IsValid() ){
		return false;
	}
	const int nLen = cStr.GetLength();
	const WCHAR* pLine = cStr.GetPtr();
	int nSharp = 0;
	while( nSharp < nLen && L'#' == pLine[nSharp] ){
		++nSharp;
	}
	if( nSharp <= 0 || 6 < nSharp || nSharp >= nLen ){
		return false;
	}
	int nText = nSharp;
	while( nText < nLen && ( L' ' == pLine[nText] || L'\t' == pLine[nText] ) ){
		++nText;
	}
	// 見出しの文字が無い（`#` と空白だけ）なら見出しにしない
	if( nText >= nLen || L'\r' == pLine[nText] || L'\n' == pLine[nText] ){
		return false;
	}
	if( pnLevel )     *pnLevel = ( 3 < nSharp ) ? 3 : nSharp;
	if( pnTextStart ) *pnTextStart = nText;
	return true;
}

//! 改行記号の手前まで
int LineEndPos( const CStringRef& cStr )
{
	const int nLen = cStr.GetLength();
	const WCHAR* pLine = cStr.GetPtr();
	for( int i = 0; i < nLen; ++i ){
		if( L'\r' == pLine[i] || L'\n' == pLine[i] ){
			return i;
		}
	}
	return nLen;
}

} // namespace

bool CColor_MdHeading::BeginColor(const CStringRef& cStr, int nPos)
{
	int nLevel = 0;
	int nTextStart = 0;
	if( !ParseHeading( cStr, &nLevel, &nTextStart ) ){
		return false;
	}
	// `#` の並びが終わって本文が始まる所から色を付ける（記号は CColor_MdMarker が消している）
	if( nPos != nTextStart || nLevel != m_nLevel ){
		return false;
	}
	m_nEnd = LineEndPos( cStr );
	return true;
}

bool CColor_MdHeading::EndColor([[maybe_unused]] const CStringRef& cStr, int nPos)
{
	return ( nPos >= m_nEnd );
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                    見出しの `#` 記号                        //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

void CColor_MdMarker::Update(void)
{
	CColorStrategy::Update();
	m_bMarkdown = false;
	const CEditDoc* pcDoc = CEditDoc::GetInstance(0);
	if( nullptr != pcDoc ){
		m_bMarkdown = IsMarkdownPath( pcDoc->m_cDocFile.GetFilePath() );
	}
}

bool CColor_MdMarker::BeginColor(const CStringRef& cStr, int nPos)
{
	if( 0 != nPos ){
		return false;
	}
	int nTextStart = 0;
	if( !ParseHeading( cStr, nullptr, &nTextStart ) ){
		return false;
	}
	m_nEnd = nTextStart;
	return true;
}

bool CColor_MdMarker::EndColor([[maybe_unused]] const CStringRef& cStr, int nPos)
{
	return ( nPos >= m_nEnd );
}

