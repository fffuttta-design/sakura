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

//! Markdown のときに足す行間（ドット・DPI補正前）
/*!
	見出し1がこのぶんだけ高くなる。大きくしすぎると本文がスカスカになるので、
	「見出しがはっきり大きい」と「本文が間延びしない」の折り合いでこの値にしている。
*/
constexpr int MD_EXTRA_LINE_SPACE = 7;

//! 見出しの段ごとの大きさの割り当て（広げた行間のうち、何割を使うか）
constexpr double MD_HEADING_RATIO[3] = { 1.0, 0.62, 0.30 };

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

#endif /* SAKURA_MARKDOWN_H_ */
