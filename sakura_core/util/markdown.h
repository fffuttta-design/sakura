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
	🔥 **見出しは「背だけ」しか大きくできない。横幅は本文と同じ升目に固定される。**
	   サクラは全部の文字を同じ大きさの升目に置くので、見出しだけ升目を広げられない。
	   ∴ ここを上げると字は**縦に伸びる**（横には広がらない）。上げすぎると細長く潰れる。

	2026-09-07 本人「今はプレーンテキストとあまり大きさが変わらない」。
	原因はまさに上の性質＝**横幅が本文と同じ**なので、縦に1.45倍あっても大きく見えなかった。
	∴ 1.45 → 1.75 へ引き上げた（3段とも上げて、段どうしの差もはっきりさせた）。

	⚠ 行の高さは**この値から自動で決まる**（`MD_LINE_HEIGHT_MARGIN`）ので、頭打ちにはならない。
	   もとは行の高さも定数で、そちらを上げ忘れると頭打ちになった
	   （`CalcHeadingHeight()` が行の高さで切る）ので、上げても効かなくなる。
	⚠ 2.10 まで描いて比べたが、その辺りから**縦長に潰れて見える**（2026-09-07）。
	   1.75 が「はっきり大きいのに字の形が崩れない」上限あたり。
*/
//! 🔥🔥 **倍率を上げるほど、字は横に潰れる。**（2026-09-08 実測して確定）
//!    升目の幅は本文のまま固定なので、**横の潰れ具合 ≒ 1 ÷ 倍率**。
//!      145% → 横 約75%（違和感が少ない） ／ 175% → 約60% ／ 210% → 約50%（縦長で「キモい」）
//!    ⚠ この比率は**本文のフォントサイズを変えても改善しない**（升目も一緒に大きくなるため）。
//!    ∴ 「見出しを大きく見せたい」の正解は倍率ではなく、**本文のフォントサイズを上げること**。
//!      升目ごと大きくなるので、見出しは縦横そろって大きくなる（本人の判断 2026-09-08）。
//!
//! 🔥 **ここは「初回の既定値」**。実際に使う値は設定画面（共通設定＞Markdown）から変わる。
//!    ＝ `GetDllShareData().m_Common.m_sMarkdown.m_nHeadingScale[]`（%）。
constexpr int MD_DEF_HEADING_SCALE[3] = { 155, 128, 118 };

//! 見出しに使う書体（入っていなければ本文と同じ書体に落とす）
/*!
	🔥 **ＭＳ ゴシックの太字は、この大きさでは使えない。**
	   1.10〜1.14倍では太字になるが、**1.20倍以上にすると太字の指定が効かなくなる**
	   （同じ太さで描かれる）。しかも太くなったときは画数の多い漢字が潰れる。
	   BIZ UDゴシックは等幅・輪郭のみで、太字にしても「戦」「略」の画が残る
	   （2026-09-01、同じ描き方で7通り描いて決めた）。

	Windows 10(1809)以降に標準で入っている。無い場合は本文と同じ書体を使う。
*/
constexpr const WCHAR* MD_DEF_HEADING_FACE = L"BIZ UDゴシック";

//! 見出しの下に空けたい余白（本文の文字の高さに対する割合）
/*!
	🔥 **見出しの行だけ升目を高くすることはできない**（サクラは全行が同じ高さ）。
	   ∴ 行の高さは本文のまま据え置き、**見出しの字を升目より上へずらして描く**ことで
	   下に余白を作る。上へ出たぶんは、1つ上の行の「字の下の空き」に収まる。

	   こうする理由（2026-09-05 本人指示）：
	   行の高さを上げて余白を作ると、**本文どうしの行間まで一緒に広がってしまう**。
	   本人の要望は「見出しのあとだけ空けて、本文どうしは従来のまま」。∴ ずらす方を採った。

	⚠ ずらせる量は「1つ上の行の字に触れない範囲」まで（`CalcHeadingLift()` が頭打ちにする）。
*/
constexpr int MD_DEF_HEADING_GAP = 40;	//!< %

//! 🔥 **行の高さは設定値ではなく、見出し1の大きさから自動で決める**（2026-09-07）。
/*!
	行が見出しより低いと見出しはそこで頭打ちになって大きくならない。
	∴ 「行の高さ」を人が触れる設定項目にすると、**見出しを大きくしても変わらない**という
	   壊れ方を自分で作れてしまう。だから設定にはせず、見出し1の大きさ＋この余裕で自動に決める。
	⚠ ここを上げると `.md` の**全部の行**の間隔が広がる（1画面に入る行が減る）。
*/
constexpr int MD_LINE_HEIGHT_MARGIN = 10;	//!< 行の高さ（%）＝ 見出し1の大きさ + これ

//! `.md` のときだけ、文字と文字のあいだに空ける隙間（px・DPI前）
/*!
	🔥 **見出しが窮屈に見えるのを直すための設定**（2026-09-07 本人「文字同士の間隔がすごく狭くて窮屈」）。

	なぜ窮屈になるか：見出しは**背だけ 1.45 倍**にしていて、**横幅は本文と同じ升目のまま**。
	升目1個の幅は変えられないので、字は縦長になり、しかも隣とぴったり接する。

	直し方は、行間でやったのとまったく同じ考え方＝**文書ぜんぶの升目を少し横に広げる**。
	全部の升目がそろって広がるので、カーソル・クリック・折り返しの計算は今までどおり。

	⚠ ここを上げると `.md` の**本文の字間も一緒に広がる**（行間を広げたときと同じ性質）。
	   0 にすれば今までとまったく同じ見た目に戻る。
*/
//! 🔥 2px は「広がりすぎてキモい」と却下された（2026-09-07 本人）。既定は 0。
constexpr int MD_DEF_CHAR_SPACING = 0;

//! 見出しの字を、升目より何px細く描くか（＝字と字のあいだに空く隙間）
/*!
	🔥 **見出しだけ字間を空けるための唯一の手**（2026-09-07 本人「見出しの文字間と
	   プレーンテキストの字間は分けよう。プレーンテキストは今まで通りでいい」）。

	なぜこれで済むか：文字の置き場所（dx配列）は **本文フォントの幅から作られていて、
	見出しフォントに切り替えても変わらない**（`CCharWidthCache` は本文フォントで1回だけ作る）。
	∴ 見出しフォントの `lfWidth` を細くしても、**升目もカーソルもクリック位置も1ミリも動かない**。
	細くしたぶんが、そのまま字と字のあいだの隙間になる。

	⚠ 単位は「半角1文字あたり」。全角の字は 2 倍効く（1 にすると全角の隙間は 2px）。
	⚠ 大きくしすぎると字が細くなりすぎる。1〜2 が実用範囲。
	⚠ `MD_CHAR_SPACING`（文書ぜんぶの字間）とは別物。あちらは**本文まで広がる**ので
	   「広がりすぎてキモい」と却下された（2026-09-07）。0 のままにしておくこと。
*/
constexpr int MD_DEF_HEADING_NARROW = 1;

//! 見出しの段ごとに、字を横へ何倍に広げるか（**分子。分母は 2**）
/*!
	🔥🔥 **これが「見出しを大きくしても潰れない」ための仕掛け。**（2026-09-08）

	サクラは1文字を升目に置く。全角1字＝升目2つ（半角2つぶん）が既定。
	背だけ伸ばすと横が足りず潰れる（横の潰れ ≒ 1 ÷ 倍率）ので、
	**見出しの1字に升目を多く使わせて、横も一緒に広げる。**

	| 値 | 全角1字が使う升目 | 実質の横倍率 | 釣り合う高さ |
	|---|---|---|---|
	| 2 | 2つ（既定） | 1.0倍 | 100〜130% |
	| 3 | 3つ | 1.5倍 | **約155%** |
	| 4 | 4つ | 2.0倍 | 約210% |

	⚠ **升目は整数個しか使えない**ので、刻めるのは 0.5倍きざみだけ。
	⚠ **高さ（MD_DEF_HEADING_SCALE）と釣り合わせること。** ずれると今度は横に伸びて見える。
	⚠ 広げると**その行に入る文字数は減る**（折り返しが早くなる）。
	⚠ 半角文字は「1つ→2つ」と切り上がる（1.5個の升目は作れない）ので、
	   見出しの中の英数字は少しゆったり並ぶ。日本語の見出しでは気にならない。
*/
constexpr int MD_HEADING_WIDTH_NUM[3] = { 3, 2, 2 };

//! 見出しの行の桁数（半角換算）を、上の倍率で広げる
/*!
	🔥 **桁を数える所は全部これを通すこと。** 通し忘れた所ができると、
	   「字は動いたのにカーソルは元の場所」になる（2026-09-07 に半日溶かした型のバグ）。
	   通す先＝CMemoryIterator::scanNext / CLayoutMgr::GetLayoutXOfChar /
	          CTextDrawer::DispText / CFigure_Text::FowardChars の4か所。
*/
inline int MdWidenColumns( int nCols, int nLevel )
{
	if( nLevel < 1 || 3 < nLevel || nCols <= 0 ){
		return nCols;
	}
	const int nNum = MD_HEADING_WIDTH_NUM[nLevel - 1];
	if( 2 == nNum ){
		return nCols;		// 広げない（今までどおり）
	}
	return ( nCols * nNum + 1 ) / 2;	// 半端は切り上げ
}


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

//! 空白（半角・タブ・全角）か
/*!
	🔥 **全角スペース（U+3000）も数える。** 日本語入力のスペースキーは全角が入るので、
	   半角しか見ないと打ったとおりにならない（Android版で実際に踏んだ 2026-09-02）。
*/
inline bool MdIsSpace( wchar_t c )
{
	return ( L' ' == c || L'\t' == c || L'\u3000' == c );
}

//! その行が区切り線（`--- `）か
/*!
	`-` が3つ以上、**そのうしろに空白が1つ以上**、あとは行末まで空白だけ。
	🔥 **空白で区切り線に切り替わる**（見出しの `# ` と同じ考え方）。
	   `---` と打っただけでは素の文字のままで、スペースを入れた瞬間に線になる。
	   ∴ 文中のハイフンの並びを勝手に線にしてしまうことがない。
	改行記号（CR/LF）は数に入れない。
*/
inline bool MdIsHorizontalRule( const wchar_t* pLine, int nLen )
{
	if( nullptr == pLine || nLen < 4 || L'-' != pLine[0] ){
		return false;
	}
	int i = 0;
	while( i < nLen && L'-' == pLine[i] ){
		++i;
	}
	if( i < 3 ){
		return false;
	}
	// うしろは空白が1つ以上。そのあとは行末まで空白だけ
	int nSpace = 0;
	for( ; i < nLen; ++i ){
		if( L'\r' == pLine[i] || L'\n' == pLine[i] ){
			break;
		}
		if( !MdIsSpace( pLine[i] ) ){
			return false;
		}
		++nSpace;
	}
	return ( 0 < nSpace );
}

//! リンク `[表示テキスト](URL)` の位置
/*!
	🔥 **画面に出すのは表示テキストだけ。** `[` と `](URL)` は
	   「幅ゼロ・描かない」で消す（見出しの `#` と同じやり方）。
	   文字そのものは残っているので、保存すればただの Markdown に戻る。
*/
struct MdLinkPos {
	int nOpen;		//!< `[` の位置
	int nTextBgn;	//!< 表示テキストの始まり（`[` の次）
	int nTextEnd;	//!< 表示テキストの終わり（`]` の位置）
	int nUrlBgn;	//!< URL の始まり（`(` の次）
	int nUrlEnd;	//!< URL の終わり（`)` の位置）
};

//! リンク1つ（`[…](…)` ぜんぶ）の長さの上限
/*!
	🔥 上限を切る理由は**速さ**。隠す文字かどうかは1文字ごとに調べるので、
	   「手前にある `[` を無制限に探す」と長い行で二乗の手間になり、打つたびに固まる。
	   ∴ ここまで遡って見つからなければリンクではない、と打ち切る。
	   **描画・レイアウト・色分け・クリックが全部この上限を共有する**ので、
	   どれか1つだけ判定が変わる（＝文字とカーソルがずれる）ことは起きない。
*/
constexpr int MD_LINK_MAX_LEN = 512;

//! 位置 nAt から始まるリンクを読む
/*!
	規則（ふたMEMO と同じ）：
	- `[表示テキスト](URL)` が途切れず並んでいること
	- 表示テキストと URL はどちらも1文字以上（空はリンクにしない）
	- 表示テキストに `[` `]` 改行を含めない／URL に空白・改行・`(` を含めない
*/
inline bool MdParseLinkAt( const wchar_t* pLine, int nLen, int nAt, MdLinkPos* pOut )
{
	if( nullptr == pLine || nAt < 0 || nAt >= nLen || L'[' != pLine[nAt] ){
		return false;
	}
	const int nStop = ( nLen < nAt + MD_LINK_MAX_LEN ) ? nLen : ( nAt + MD_LINK_MAX_LEN );
	int i = nAt + 1;
	while( i < nStop && L']' != pLine[i] ){
		if( L'\r' == pLine[i] || L'\n' == pLine[i] || L'[' == pLine[i] ){
			return false;
		}
		++i;
	}
	if( i >= nStop || i == nAt + 1 ){
		return false;			// `]` が無い／表示テキストが空
	}
	const int nTextEnd = i;
	if( nTextEnd + 1 >= nLen || L'(' != pLine[nTextEnd + 1] ){
		return false;			// `]` のすぐ後ろが `(` でない
	}
	int j = nTextEnd + 2;
	while( j < nStop && L')' != pLine[j] ){
		if( L'\r' == pLine[j] || L'\n' == pLine[j] || L'(' == pLine[j]
		 || L'[' == pLine[j] || L']' == pLine[j] || MdIsSpace( pLine[j] ) ){
			return false;
		}
		++j;
	}
	if( j >= nStop || j == nTextEnd + 2 ){
		return false;			// `)` が無い／URL が空
	}
	if( pOut ){
		pOut->nOpen    = nAt;
		pOut->nTextBgn = nAt + 1;
		pOut->nTextEnd = nTextEnd;
		pOut->nUrlBgn  = nTextEnd + 2;
		pOut->nUrlEnd  = j;
	}
	return true;
}

//! nFrom 以降で最初のリンクを探す
inline bool MdFindNextLink( const wchar_t* pLine, int nLen, int nFrom, MdLinkPos* pOut )
{
	if( nullptr == pLine || nLen <= 0 ){
		return false;
	}
	for( int p = ( 0 < nFrom ? nFrom : 0 ); p < nLen; ++p ){
		if( L'[' == pLine[p] && MdParseLinkAt( pLine, nLen, p, pOut ) ){
			return true;
		}
	}
	return false;
}

//! 位置 nAt が「リンクの表示テキストの中」なら、そのリンクを返す
inline bool MdFindLinkAtText( const wchar_t* pLine, int nLen, int nAt, MdLinkPos* pOut )
{
	MdLinkPos link;
	int p = 0;
	while( MdFindNextLink( pLine, nLen, p, &link ) ){
		if( link.nTextBgn <= nAt && nAt < link.nTextEnd ){
			if( pOut ){ *pOut = link; }
			return true;
		}
		if( nAt < link.nOpen ){
			break;
		}
		p = link.nUrlEnd + 1;
	}
	return false;
}

//! 位置 nAt に関わっているリンク（記号の上・すぐ後ろも含む）を返す
/*!
	Ctrl+K でリンクを編集するときに使う。カーソルが表示テキストの上にあるとき、
	リンクの直後（`)` の次）にあるときも「そのリンク」とみなす。
*/
inline bool MdFindLinkCovering( const wchar_t* pLine, int nLen, int nAt, MdLinkPos* pOut )
{
	MdLinkPos link;
	int p = 0;
	while( MdFindNextLink( pLine, nLen, p, &link ) ){
		if( link.nOpen <= nAt && nAt <= link.nUrlEnd + 1 ){
			if( pOut ){ *pOut = link; }
			return true;
		}
		if( nAt < link.nOpen ){
			break;
		}
		p = link.nUrlEnd + 1;
	}
	return false;
}

//! 行頭が見出しなら、段（1～3）と本文の始まる位置を返す
/*!
	`#` は 1～6 個。**そのうしろの空白は、あっても無くてもよい**
	（`# 見出し` でも `#見出し` でも見出しになる。2026-09-07 本人指示）。
	空白があれば記号ごと飲み込むので、本文には含めない。
	`# ` だけの行（本文がまだ無い・全部消した）も見出しとして扱う。
	🔥 **`#` だけの行は見出しにしない。** `#` を打った瞬間に見出しになると、
	   その `#` 自身が隠れて画面から消え、何も起きていないように見える。
	   ∴ うしろに空白か文字が1つ来た時点で見出しに切り替わる。
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
	// 🔥 全角スペース（U+3000）も空白として数える。日本語入力のスペースキーは全角が入るので、
	//    半角しか見ないと「# 」と打っても見出しにならない（Android版で実際に踏んだ 2026-09-02）。
	while( nText < nLen && nText < MD_MARKER_MAX && MdIsSpace( pLine[nText] ) ){
		++nText;
	}
	// 🔥 **空白はあっても無くてもよい**（`# 見出し` でも `#見出し` でも見出し）。
	//    以前は空白を必須にしていたが、その空白を Backspace で消すと
	//    見出しが丸ごと解けてしまっていた（2026-09-07 本人から指摘）。
	//    見出しのためだけに空白を 1 文字抱えるのも無駄なので、任意に戻した。
	// ⚠ ただし **`#` だけの行は見出しにしない**。
	//    ここを見出しにすると、`#` を打った瞬間にその `#` が消えてしまい、
	//    何も起きていないように見える（空白を必須にしていた元々の理由）。
	if( nText == nSharp ){
		if( nText >= nLen || L'\r' == pLine[nText] || L'\n' == pLine[nText] ){
			return false;			// `#` のうしろに何も無い
		}
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

//! 太字 `**文字**` の位置
/*!
	🔥 画面に出すのは中の文字だけ。前後の `**` は「幅ゼロ・描かない」で消す。
	   ふたMEMO と同じ見え方（記号は消えて、字が太くなる）。
*/
struct MdBoldPos {
	int nOpenBgn;	//!< 前の `**` の開始
	int nTextBgn;	//!< 中の文字の始まり
	int nTextEnd;	//!< 中の文字の終わり（後ろの `**` の開始）
	int nCloseEnd;	//!< 後ろの `**` の終わり（次の文字の位置）
};

//! 位置 nAt から始まる太字を読む
/*!
	規則：`**` のあと1文字以上（`*` と改行は不可）、そのあと `**`。
	中身が空（`****`）は太字にしない。
	🔥 **Ctrl+B を押しただけでは `****` を入れない**（入れると中身が無いまま残る）。
	   押した時点では「次に打つ字を太字にする」印だけを立て、字が来たときに `**字**` を作る。
*/
inline bool MdParseBoldAt( const wchar_t* pLine, int nLen, int nAt, MdBoldPos* pOut )
{
	if( nullptr == pLine || nAt < 0 || nAt + 4 > nLen ){
		return false;
	}
	if( L'*' != pLine[nAt] || L'*' != pLine[nAt + 1] ){
		return false;
	}
	const int nStop = ( nLen < nAt + MD_LINK_MAX_LEN ) ? nLen : ( nAt + MD_LINK_MAX_LEN );
	int i = nAt + 2;
	while( i + 1 < nStop ){
		if( L'\r' == pLine[i] || L'\n' == pLine[i] ){
			return false;
		}
		if( L'*' == pLine[i] ){
			if( L'*' != pLine[i + 1] ){
				return false;		// `*` 単体は太字にしない
			}
			break;					// 閉じの `**` が来た
		}
		++i;
	}
	if( i + 1 >= nStop || L'*' != pLine[i] || L'*' != pLine[i + 1] ){
		return false;				// 閉じが無い
	}
	if( i == nAt + 2 ){
		return false;				// 中身が空（`****`）は太字にしない
	}
	if( pOut ){
		pOut->nOpenBgn = nAt;
		pOut->nTextBgn = nAt + 2;
		pOut->nTextEnd = i;
		pOut->nCloseEnd = i + 2;
	}
	return true;
}

//! nFrom 以降で最初の太字を探す
inline bool MdFindNextBold( const wchar_t* pLine, int nLen, int nFrom, MdBoldPos* pOut )
{
	if( nullptr == pLine || nLen <= 0 ){
		return false;
	}
	for( int p = ( 0 < nFrom ? nFrom : 0 ); p + 1 < nLen; ++p ){
		if( L'*' == pLine[p] && MdParseBoldAt( pLine, nLen, p, pOut ) ){
			return true;
		}
	}
	return false;
}

//! 位置 nAt に関わっている太字（記号の上・すぐ後ろも含む）を返す
inline bool MdFindBoldCovering( const wchar_t* pLine, int nLen, int nAt, MdBoldPos* pOut )
{
	MdBoldPos bold;
	int p = 0;
	while( MdFindNextBold( pLine, nLen, p, &bold ) ){
		if( bold.nOpenBgn <= nAt && nAt <= bold.nCloseEnd ){
			if( pOut ){ *pOut = bold; }
			return true;
		}
		if( nAt < bold.nOpenBgn ){
			break;
		}
		p = bold.nCloseEnd;
	}
	return false;
}

//! 位置 i を含む「隠す範囲」の終わりを返す（隠さないなら i をそのまま返す）
/*!
	🔥 **隠す＝「幅ゼロにする」＋「描かない」。** この2つは必ず同じ範囲でやること。
	   片方だけ直すと、文字とカーソル・クリック位置がずれる。
	   ∴ 隠す範囲の判定はこの関数1本だけを見る（レイアウト・描画の両方から呼ぶ）。

	隠すもの：
	- 行頭の見出し記号（`#` ＋ うしろの空白）
	- リンクの `[` と `](URL)`

	@param pLine 行の**先頭**から。途中を指すポインタを渡すと行頭判定を誤る。
*/
inline int MdHiddenEndAt( const wchar_t* pLine, int nLen, int i )
{
	if( nullptr == pLine || i < 0 || i >= nLen ){
		return i;
	}
	// 見出しの記号
	if( i < MD_MARKER_MAX && L'#' == pLine[0] ){
		const int nMarker = MdHeadingMarkerLen( pLine, nLen );
		if( i < nMarker ){
			return nMarker;
		}
	}
	// リンクの記号と URL
	//   🔥 手前にある **いちばん近い `[`** だけを見る。表示テキストにも URL にも
	//      `[` を入れない規則にしてあるので、これで取りこぼさない。
	//      行の頭から探し直すと、1文字ごとに行ぜんぶを走査することになって重い。
	const int nBack = ( i > MD_LINK_MAX_LEN ) ? ( i - MD_LINK_MAX_LEN ) : 0;
	for( int p = i; p >= nBack; --p ){
		if( L'[' != pLine[p] ){
			continue;
		}
		MdLinkPos link;
		if( !MdParseLinkAt( pLine, nLen, p, &link ) ){
			break;							// 手前の `[` はリンクではない
		}
		if( link.nOpen == i ){
			return link.nTextBgn;			// `[` を隠す
		}
		if( link.nTextEnd <= i && i <= link.nUrlEnd ){
			return link.nUrlEnd + 1;		// `](URL)` を隠す
		}
		break;								// 表示テキストの中＝隠さない
	}
	// 太字の `**`
	//   🔥 リンクと同じ考え方で、手前から近い順に「開きの `**`」を探す。
	for( int p = i; p >= nBack; --p ){
		if( p + 1 >= nLen || L'*' != pLine[p] || L'*' != pLine[p + 1] ){
			continue;
		}
		MdBoldPos bold;
		if( !MdParseBoldAt( pLine, nLen, p, &bold ) ){
			continue;						// ここは開きではない（閉じの `**` など）
		}
		if( bold.nCloseEnd <= i ){
			break;							// i より前で閉じている＝関係ない
		}
		if( i < bold.nTextBgn ){
			return bold.nTextBgn;			// 前の `**` を隠す
		}
		if( bold.nTextEnd <= i ){
			return bold.nCloseEnd;			// 後ろの `**` を隠す
		}
		break;								// 中の文字＝隠さない
	}
	return i;
}

//! 行が見出しならその段（1〜3）、違えば 0（【自前改造】）
/*!
	🔥 **必ず「行の先頭」から渡すこと。** 途中を指すポインタだと行頭判定を誤る。
	折り返した行でも、判定に使うのは**論理行の先頭**（見出しは行ごとの性質なので、
	折り返した2行目以降も同じ幅で数えないと桁がずれる）。
*/
inline int MdHeadingLevelOf( const wchar_t* pLine, int nLen )
{
	int nLevel = 0;
	return MdParseHeading( pLine, nLen, &nLevel, nullptr ) ? nLevel : 0;
}

#endif /* SAKURA_MARKDOWN_H_ */
