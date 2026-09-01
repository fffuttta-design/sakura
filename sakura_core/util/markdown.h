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
*/
constexpr double MD_HEADING_SCALE[3] = { 1.25, 1.14, 1.07 };

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

#endif /* SAKURA_MARKDOWN_H_ */
