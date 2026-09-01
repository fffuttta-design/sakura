/*! @file
	@brief 【自前改造】Markdown かどうかの判定と、見出しを大きく描くための寸法

	🔥 サクラは全行を同じ高さの升目に文字を置く。1行だけ高くはできない。
	   ∴ 見出しを大きくする手は「文書ぜんぶの行間を広げて、見出しだけその高さいっぱいに描く」。
	   全行そろって広がるので、レイアウト・カーソル・スクロールの計算は今までどおりでよい。

	   幅は升目のまま（＝少し縦長になる）。横も広げると隣の字と重なるうえ、
	   クリック位置と文字位置がずれる（升目1個＝1文字という前提が崩れる）。

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_MARKDOWN_H_
#define SAKURA_MARKDOWN_H_
#pragma once

#include <Windows.h>

//! 見出しの段ごとの倍率（本文の文字の高さに対して）
/*!
	🔥 **字の形を歪めない**ことを優先して、この程度に抑えている。

	日本語は「全角＝高さと同じ幅」で作られているので、背を高くすると同じだけ横にも太る。
	升目1個の幅は変えられない（変えるとクリック位置と文字位置がずれる）ので、
	大きくしすぎると隣の字と重なる。逆に幅を升目に押し込めると**細長く潰れて気持ち悪い**
	（2026-09-01、1.5倍で試して本人から「キモい」と指摘。1.25倍・形はそのままに直した）。

	1.25倍なら、はみ出しは全角1文字あたり2ドットほど。字の左右の余白でほぼ吸収される。

	3段目は本文と同じ大きさ（1.00倍）。**太字にするので大きさを変えなくても見分けが付く。**
*/
constexpr double MD_HEADING_SCALE[3] = { 1.25, 1.14, 1.00 };

//! 見出しに使う書体（入っていなければ本文と同じ書体に落とす）
/*!
	🔥 **ＭＳ ゴシックの太字は、この大きさでは使えない。**
	   1.10〜1.14倍では太字になるが、**1.20倍以上にすると太字の指定が効かなくなる**
	   （同じ太さで描かれる）。しかも太くなったときは画数の多い漢字が潰れる。
	   BIZ UDゴシックは等幅・輪郭のみで、太字にしても「戦」「略」の画が残る
	   （2026-09-01、同じ描き方で7通り描いて決めた）。

	Windows 10(1809)以降に標準で入っている。無い場合は本文と同じ書体を使う。
*/
constexpr const WCHAR* MD_HEADING_FACE = L"BIZ UDゴシック";

//! いちばん大きい見出しが収まるように、行の高さをこの倍率まで広げる
constexpr double MD_LINE_HEIGHT_SCALE = 1.32;

//! 拡張子が Markdown か
inline bool IsMarkdownPath( const WCHAR* pszPath )
{
	if( nullptr == pszPath ){
		return false;
	}
	const WCHAR* pszExt = ::wcsrchr( pszPath, L'.' );
	if( nullptr == pszExt ){
		return false;
	}
	return ( 0 == ::_wcsicmp( pszExt, L".md" ) )
	    || ( 0 == ::_wcsicmp( pszExt, L".markdown" ) );
}

//! 行頭の `#` を隠すときに飲み込む最大の長さ（`#`×6 ＋ うしろの空白）
/*!
	空白を無制限に数えると、空白だらけの長い行で 1 文字ごとに行頭から数え直すことになり、
	描画が重くなる。実用上ここで足りるので上限を切ってある。
*/
constexpr int MD_MARKER_MAX = 16;

//! 行頭が見出しなら、段（1～3）と本文の始まる位置を返す
/*!
	`#` は 1～6 個。そのうしろの空白は本文に含めない（記号ごと飲み込むため）。
	🔥 空白は要求しない。本人は `#見出し` と詰めて書く（2026-09-01）。
	   行頭でしか見ないので、文中のハッシュタグを拾う心配は無い。

	🔥 **判定は必ずこの関数だけを使う。**
	   色分け・描画・レイアウト（幅ゼロ）の3か所で同じ範囲を指さないと、
	   文字とカーソルがずれる。
*/
inline bool MdParseHeading( const wchar_t* pLine, int nLen, int* pnLevel, int* pnTextStart )
{
	if( nullptr == pLine || nLen <= 0 || L'#' != pLine[0] ){
		return false;
	}
	int nSharp = 0;
	while( nSharp < nLen && L'#' == pLine[nSharp] ){
		++nSharp;
	}
	if( nSharp <= 0 || 6 < nSharp || nSharp >= nLen ){
		return false;
	}
	int nText = nSharp;
	while( nText < nLen && nText < MD_MARKER_MAX
	    && ( L' ' == pLine[nText] || L'\t' == pLine[nText] ) ){
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

//! 行頭の見出し記号（`#` ＋ うしろの空白）の長さ。見出しでなければ 0
inline int MdHeadingMarkerLen( const wchar_t* pLine, int nLen )
{
	int nTextStart = 0;
	return MdParseHeading( pLine, nLen, nullptr, &nTextStart ) ? nTextStart : 0;
}

#endif /* SAKURA_MARKDOWN_H_ */
