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
	const WCHAR* pszExt = ::wcsrchr( pszPath, L'.' );
	if( nullptr == pszExt ){
		return;
	}
	m_bMarkdown = ( 0 == ::_wcsicmp( pszExt, L".md" ) )
	           || ( 0 == ::_wcsicmp( pszExt, L".markdown" ) );
}

bool CColor_MdHeading::BeginColor(const CStringRef& cStr, int nPos)
{
	if( !cStr.IsValid() ){
		return false;
	}
	if( 0 != nPos ){
		return false;	// 見出しは行の先頭からしか始まらない
	}
	const int nLen = cStr.GetLength();
	const WCHAR* pLine = cStr.GetPtr();

	// 先頭の `#` を数える
	int nSharp = 0;
	while( nSharp < nLen && L'#' == pLine[nSharp] ){
		++nSharp;
	}
	if( nSharp <= 0 ){
		return false;
	}
	// `#` だけの行は見出しにしない（見出しの文字が要る）
	if( nSharp >= nLen ){
		return false;
	}
	// 🔥 `#` のうしろの空白は要求しない。
	//    Markdown の決まりでは空白が要るが、メモでは `#見出し` と詰めて書くのが普通で、
	//    厳密にすると本人の書き方が色分けされない（2026-09-01 実際にそうなっていた）。
	//    行頭でしか見ないので、文中のハッシュタグを拾う心配は無い。
	if( L'\r' == pLine[nSharp] || L'\n' == pLine[nSharp] ){
		return false;
	}
	// 4段目より下は3段目と同じ扱いにする（色を増やしても見分けが付かないため）
	const int nLevel = ( 6 < nSharp ) ? 0 : ( ( 3 < nSharp ) ? 3 : nSharp );
	if( nLevel != m_nLevel ){
		return false;
	}

	// 改行記号まで色を塗ると記号まで見出し色になるので、その手前で止める
	int nEnd = nLen;
	for( int i = 0; i < nLen; ++i ){
		if( L'\r' == pLine[i] || L'\n' == pLine[i] ){
			nEnd = i;
			break;
		}
	}
	m_nEnd = nEnd;
	return true;
}

bool CColor_MdHeading::EndColor([[maybe_unused]] const CStringRef& cStr, int nPos)
{
	return ( nPos >= m_nEnd );
}
