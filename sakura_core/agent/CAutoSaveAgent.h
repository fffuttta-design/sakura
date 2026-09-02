/*!	@file
	@brief ファイルの自動保存

	@author genta
	@date 2000
*/
/*
	Copyright (C) 2000-2001, genta
	Copyright (C) 2018-2022, Sakura Editor Organization

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_CAUTOSAVEAGENT_AB1DD112_42B8_4A93_8E04_C2889F16DC53_H_
#define SAKURA_CAUTOSAVEAGENT_AB1DD112_42B8_4A93_8E04_C2889F16DC53_H_
#pragma once

#include <Windows.h>
#include "_main/global.h"
#include "doc/CDocListener.h"

//! 分→ミリ秒に変換するための係数
const int MSec2Min = 1000 * 60;

//! 【自前改造】自動保存するまでの「手が止まっている時間」(ミリ秒)
/*!
	打鍵のたびに保存すると、置き場所（Googleドライブ）の同期が休みなく走る。
	∴ **入力が止まってから**保存する。長い文章を書いている間は保存されない。
	⚠ 短くしすぎると変換の途中でも保存が走る。3秒は「一息ついた」と言える長さ。
*/
constexpr DWORD AUTOSAVE_IDLE_MS = 3000;

/*! @class CPassiveTimer CAutoSave.h
	基準時刻からの経過時間が設定間隔を過ぎたかどうかを判定する。
	頻繁に呼び出されるタイマーが既に別の場所にあるとき、それよりも間隔が広くて
	間隔の厳密さが要求されない用途に利用可能。
	ファイルの自動保存で使っている。
	@author genta
*/
class CPassiveTimer {
public:
	/*!
		初期値は間隔1msecでタイマーは無効。
	*/
	CPassiveTimer() : nInterval(1), bEnabled(false){ Reset(); }

	//時間間隔
	void SetInterval(int m);	//!	時間間隔の設定
	int GetInterval(void) const {return nInterval / MSec2Min; }	//!< 時間間隔の取得
	void Reset(void){ nLastTick = ::GetTickCount(); }			//!< 基準時刻のリセット

	//有効／無効
	void Enable(bool flag);							//!< 有効／無効の設定
	bool IsEnabled(void) const { return bEnabled; }	//!< 有効／無効の読み出し

	//!	規定時間に達したかどうかの判定
	bool CheckAction(void);

private:
	DWORD	nLastTick;	//!< 最後にチェックしたときの時刻 (GetTickCount()で取得したもの)
	int		nInterval;	//!< Action間隔 (分)
	bool	bEnabled;	//!< 有効かどうか
};

class CAutoSaveAgent : public CDocListenerEx{
public:
	void CheckAutoSave();
	void ReloadAutoSaveParam();	//!< 設定をSharedAreaから読み出す

	//! 【自前改造】「手が止まったら」保存する。500ms ごとに呼ばれる
	void CheckIdleAutoSave();

private:
	CPassiveTimer m_cPassiveTimer;
	//! 【自前改造】保存に失敗したときの打鍵時刻。次に書き換えられるまで再挑戦しない
	//   （読み取り専用などで失敗すると、0.5秒ごとにエラーが出続けてしまうため）
	DWORD m_dwIdleFailedTick = 0;
	bool  m_bIdleSaveFailed  = false;
};
#endif /* SAKURA_CAUTOSAVEAGENT_AB1DD112_42B8_4A93_8E04_C2889F16DC53_H_ */
