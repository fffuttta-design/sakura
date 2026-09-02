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
	🔥 **1.45倍が上限。** 升目1個の幅は変えられないので、それを超えると横に押し込まれて
	   細長く潰れる（2026-09-01、1.85倍まで描いて確かめた。以前 1.5倍・ＭＳ ゴシックで
	   試したときは本人から「キモい」と指摘）。

	なぜ 1.45 まで平気か：日本語の全角1文字ぶんの升目は「幅16px・高さ13px」のように
	**横のほうが広い**。∴ 1.2倍あたりまでは横の余りに収まり、まったく歪まない。
	1.45倍でも詰まりは14%ほどで、字の形はほぼそのまま。

	3段目も本文より大きくする（2026-09-01 本人「見出し123それぞれもう少し大きく」）。
*/
constexpr double MD_HEADING_SCALE[3] = { 1.45, 1.28, 1.15 };

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
/*!
	🔥 **いちばん大きい見出しの倍率より必ず大きくしておく。**
	   行の高さが足りないと、見出しはそこで頭打ちになって大きくならない
	   （上下が欠けないよう `UpdateHeadingFonts()` が切り詰めるため）。
	   ⚠ ここを上げると `.md` の**全部の行**の間隔が広がる。これがこの仕掛けの代償。
*/
constexpr double MD_LINE_HEIGHT_SCALE = 1.52;

//! 【自前改造】メモを自動保存するまでの「手が止まっている時間」(ミリ秒)
/*!
	打鍵のたびに保存すると、置き場所（Googleドライブ）の同期が休みなく走る。
	∴ **入力が止まってから**保存する。長い文章を書いている間は保存されない。
	⚠ 短くしすぎると変換の途中でも保存が走る。3秒は「一息ついた」と言える長さ。
*/
constexpr DWORD MD_AUTOSAVE_IDLE_MS = 3000;

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
	`#` は 1～6 個。**そのうしろに空白が1つ以上必要**（ふたMEMO と同じ規則）。
	その空白は本文に含めない（記号ごと飲み込むため）。
	`# ` だけの行（本文がまだ無い・全部消した）も見出しとして扱う。
	🔥 **空白で見出しに切り替わる。** `#` を打っただけでは記号のまま見えていて、
	   スペースを入れた瞬間に見出しになり、記号ごと消える（2026-09-01 本人指示）。
	   ∴ `#見出し` と詰めて書いても見出しにはならない。
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
	if( nSharp <= 0 || 6 < nSharp ){
		return false;
	}
	int nText = nSharp;
	while( nText < nLen && nText < MD_MARKER_MAX
	    && ( L' ' == pLine[nText] || L'\t' == pLine[nText] ) ){
		++nText;
	}
	// 🔥 空白が1つも無ければ見出しにしない（`#見出し` は素のまま）
	if( nText == nSharp ){
		return false;
	}
	// 🔥 **`#` と空白だけの行も見出しとして扱う。**
	//    ここで「文字が無ければ見出しではない」としていたため、見出しの本文を全部消すと
	//    隠していた `#` が生きかえって画面に出てきた（2026-09-01 本人から指摘）。
	//    一度見出しにしたものは、中身が空でも見出しのまま＝記号は出さない。
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
