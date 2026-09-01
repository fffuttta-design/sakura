/*! @file
	@brief 【自前改造】Markdown の見出し（# ## ###）を色分けする

	.md / .markdown を開いているときだけ働く。行の先頭が `# ` `## ` `### ` なら、
	その行ぜんぶを見出しの色で描く。

	🔥 文字を大きくすることはできない。サクラは全行を同じ高さ・同じ幅の升目に
	   文字を置く作りなので、1行だけ大きくすると桁の計算が全部ずれる。
	   ∴「色と太字で段の違いを見せる」ところまでをやる。

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_CCOLOR_MDHEADING_H_
#define SAKURA_CCOLOR_MDHEADING_H_
#pragma once

#include "view/colors/CColorStrategy.h"

//! 【自前改造】いま開いているのが Markdown か（正本＝CEditDoc::IsMarkdownDocument）
/*!
	🔥 ここで拡張子を調べ直さない。判定を持つ場所が増えると必ず食い違い、
	   「文字は左に寄っているのにカーソルだけ右」という壊れ方をする。
*/
bool MdIsCurrentDocMarkdown();

class CColor_MdHeading final : public CColorStrategy{
public:
	//! @param nLevel 見出しの段（1〜3。3は「### 以下ぜんぶ」）
	CColor_MdHeading( int nLevel, EColorIndexType eColorIndex )
		: m_nLevel( nLevel ), m_eColorIndex( eColorIndex ) { }
	EColorIndexType GetStrategyColor() const override{ return m_eColorIndex; }
	void InitStrategyStatus() override{ m_nEnd = 0; }
	bool BeginColor(const CStringRef& cStr, int nPos) override;
	bool EndColor(const CStringRef& cStr, int nPos) override;
	bool Disp() const override{
		return MdIsCurrentDocMarkdown() && m_pTypeData->m_ColorInfoArr[m_eColorIndex].m_bDisp;
	}

private:
	int					m_nLevel;
	EColorIndexType		m_eColorIndex;
	int					m_nEnd = 0;				//!< 色を終える位置
};
//! 見出しの `#` 記号を「見えなく」する
/*!
	背景と同じ色で描くので、記号は消えたように見える（ふたMEMO の見え方に寄せた）。
	⚠ 升目は消せないので、その分だけ見出しが右へずれる。文字そのものを消すと
	   クリック位置と文字位置がずれるため、ここは色で消すのが正解。
*/
class CColor_MdMarker final : public CColorStrategy{
public:
	EColorIndexType GetStrategyColor() const override{ return COLORIDX_MDMARK; }
	void InitStrategyStatus() override{ m_nEnd = 0; }
	bool BeginColor(const CStringRef& cStr, int nPos) override;
	bool EndColor(const CStringRef& cStr, int nPos) override;
	bool Disp() const override{
		return MdIsCurrentDocMarkdown() && m_pTypeData->m_ColorInfoArr[COLORIDX_MDMARK].m_bDisp;
	}

private:
	int		m_nEnd = 0;
};

#endif /* SAKURA_CCOLOR_MDHEADING_H_ */
