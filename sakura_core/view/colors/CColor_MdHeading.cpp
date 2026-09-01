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

//! 【自前改造】いま開いているのが Markdown か（正本を1か所だけ見る）
bool MdIsCurrentDocMarkdown()
{
	const CEditDoc* pcDoc = CEditDoc::GetInstance(0);
	return ( nullptr != pcDoc ) && pcDoc->IsMarkdownDocument();
}

namespace {

//! 行頭が見出しなら、段と本文の始まる位置を返す
/*!
	🔥 判定の中身は util/markdown.h に1本化してある。
	   描画（記号を飛ばす）とレイアウト（幅ゼロ）も同じ関数を見ているので、
	   ここだけ条件を変えると文字とカーソルがずれる。
*/
bool ParseHeading( const CStringRef& cStr, int* pnLevel, int* pnTextStart )
{
	if( !cStr.IsValid() ){
		return false;
	}
	return MdParseHeading( cStr.GetPtr(), cStr.GetLength(), pnLevel, pnTextStart );
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

