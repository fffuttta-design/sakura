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
		return m_bMarkdown && m_pTypeData->m_ColorInfoArr[m_eColorIndex].m_bDisp;
	}
	void Update(void) override;

private:
	int					m_nLevel;
	EColorIndexType		m_eColorIndex;
	bool				m_bMarkdown = false;	//!< 今開いているのが Markdown か
	int					m_nEnd = 0;				//!< 色を終える位置
};
#endif /* SAKURA_CCOLOR_MDHEADING_H_ */
