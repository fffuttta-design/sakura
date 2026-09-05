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
	//   🔥 「ちょうど本文の頭」ではなく「本文の頭より後ろならどこでも」始める。
	//      見出しの中にリンクがあると、リンクの記号を飛ばすせいで本文の頭の位置を通らず、
	//      見出しの色と大きさが丸ごと効かなくなる（2026-09-05 実際に起きた）。
	if( nPos < nTextStart || nLevel != m_nLevel ){
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
	// 【自前改造】区切り線（`--- `）は行ぜんぶを消す。
	//   線そのものは CEditView::DrawLayoutLine が引く（文字は残しつつ見えなくする）。
	if( cStr.IsValid() && MdIsHorizontalRule( cStr.GetPtr(), cStr.GetLength() ) ){
		m_nEnd = LineEndPos( cStr );
		return true;
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

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                    リンクの表示テキスト                     //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CColor_MdLink::BeginColor(const CStringRef& cStr, int nPos)
{
	// 🔥 行の頭から探し直さない。表示テキストは必ず `[` の次から始まるので、
	//    1文字だけ後ろを見れば足りる（毎文字ごとに行を走査すると描画が重くなる）。
	if( !cStr.IsValid() || nPos <= 0 ){
		return false;
	}
	const wchar_t* pLine = cStr.GetPtr();
	const int nLen = cStr.GetLength();
	if( L'[' != pLine[nPos - 1] ){
		return false;
	}
	MdLinkPos link;
	if( !MdParseLinkAt( pLine, nLen, nPos - 1, &link ) ){
		return false;
	}
	m_nEnd = link.nTextEnd;
	return true;
}

bool CColor_MdLink::EndColor([[maybe_unused]] const CStringRef& cStr, int nPos)
{
	return ( nPos >= m_nEnd );
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //
//                        太字の中の文字                       //
// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- //

bool CColor_MdBold::BeginColor(const CStringRef& cStr, int nPos)
{
	// 中の文字は必ず `**` の次から始まるので、2文字だけ手前を見れば足りる
	if( !cStr.IsValid() || nPos < 2 ){
		return false;
	}
	const wchar_t* pLine = cStr.GetPtr();
	if( L'*' != pLine[nPos - 1] || L'*' != pLine[nPos - 2] ){
		return false;
	}
	MdBoldPos bold;
	if( !MdParseBoldAt( pLine, cStr.GetLength(), nPos - 2, &bold ) ){
		return false;
	}
	m_nEnd = bold.nTextEnd;
	return true;
}

bool CColor_MdBold::EndColor([[maybe_unused]] const CStringRef& cStr, int nPos)
{
	return ( nPos >= m_nEnd );
}

