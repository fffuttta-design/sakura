/*! @file
	@brief 【自前改造】メモを上書きする直前に、その中身を変更履歴へ残す

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_CNOTEHISTORYAGENT_H_
#define SAKURA_CNOTEHISTORYAGENT_H_
#pragma once

#include "doc/CDocListener.h"

//! メモの変更履歴を残す係
/*!
	サイドバーに出るメモ（ノートフォルダーの中のファイル）を上書きするとき、
	**上書きされる前の中身**を履歴として残す。詳細は util/notehistory.h。
*/
class CNoteHistoryAgent : public CDocListenerEx{
public:
	ECallbackResult OnPreBeforeSave(SSaveInfo* pSaveInfo) override;
};

#endif /* SAKURA_CNOTEHISTORYAGENT_H_ */
