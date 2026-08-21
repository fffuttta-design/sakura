/*! @file
	@brief 【自前改造】SakuraEditorPlus の自動更新／手動更新

	配信元は Google ドライブの配布フォルダー。母艦もサブ機も H: を同期しているので、
	ネットワーク越しの配信サーバーを立てずに「フォルダーを見に行くだけ」で済ませる。
	版の比較は exe のファイルバージョン（第4要素がコミット数で単調増加する）。

	振る舞いは C:\dev\アプリ共通仕様.md 第4部に合わせる。
	  ・起動3秒後 ＋ 以降3分ごとに確認
	  ・新しい版があったら「今すぐ再起動して適用しますか？」と聞く
	  ・手動の「今すぐ最新版を確認」も必ず置く

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_UPDATER_H_
#define SAKURA_UPDATER_H_
#pragma once

#include <string>

//! 版（ファイルバージョン a.b.c.d）
struct SAppVersion {
	int m_nMajor = 0;
	int m_nMinor = 0;
	int m_nPatch = 0;
	int m_nBuild = 0;

	bool IsValid() const noexcept {
		return 0 != (m_nMajor | m_nMinor | m_nPatch | m_nBuild);
	}
	//! 新しければ true
	bool operator > ( const SAppVersion& other ) const noexcept {
		if( m_nMajor != other.m_nMajor ) return m_nMajor > other.m_nMajor;
		if( m_nMinor != other.m_nMinor ) return m_nMinor > other.m_nMinor;
		if( m_nPatch != other.m_nPatch ) return m_nPatch > other.m_nPatch;
		return m_nBuild > other.m_nBuild;
	}
	std::wstring ToString() const;
};

//! 配布フォルダー（...\SakuraEditorPlus\配布\app）を返す。無ければ空文字列
std::wstring GetUpdateDistDir();

//! 指定 exe のファイルバージョンを読む
SAppVersion GetExeVersion( const WCHAR* pszExePath );

//! 今動いている自分自身の版
SAppVersion GetRunningVersion();

//! 配布されている版（配布元が見つからなければ IsValid()==false）
SAppVersion GetDistVersion();

//! 更新の確認結果
struct SUpdateCheckResult {
	bool			m_bDistFound  = false;	//!< 配布元が見つかったか
	bool			m_bAvailable  = false;	//!< 新しい版があるか
	SAppVersion		m_cRunning;				//!< 今の版
	SAppVersion		m_cDist;				//!< 配布されている版
};

//! 更新があるか調べる（ファイルを見るだけ。数ミリ秒）
SUpdateCheckResult CheckUpdate();

//! 最終確認日時を記録する／読む（配布フォルダーではなく自分の導入先に置く）
void         SetLastUpdateCheckTime();
std::wstring GetLastUpdateCheckTime();

/*!
	更新を実行する

	動いている exe は上書きできないので、いったん外の小さな更新スクリプトに任せる。
	スクリプトは「全プロセスが終わるまでコピーを再試行 → 終わったら起動し直す」だけ。

	@retval true  更新スクリプトを起動した（呼び出し側は速やかに終了すること）
	@retval false 起動できなかった
*/
bool StartUpdate();

#endif /* SAKURA_UPDATER_H_ */
