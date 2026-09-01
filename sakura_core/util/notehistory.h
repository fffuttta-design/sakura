/*! @file
	@brief 【自前改造】メモの変更履歴（上書きする直前の中身を残す）

	ノートフォルダーの中の隠しフォルダー `.履歴\<メモのファイル名>\` に、
	`YYYY-MM-DD_HHMMSS.txt` という名前でその時点の中身を残す。

	🔥 なぜ要るか：2026-09-01、自動更新でサクラを終了させた拍子に、開いていたメモが
	   1行目だけに切り詰められて保存された（14,649バイト → 23バイト）。
	   Google ドライブの版履歴から拾えたが、アプリの中から戻せる道が無かった。

	置き場所をメモと同じフォルダーにしているのは、**ドライブがそのまま2台へ配ってくれる**から。
	（サーバーも設定も要らない。ノートの一覧には出ない＝フォルダーだから）

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_NOTEHISTORY_H_
#define SAKURA_NOTEHISTORY_H_
#pragma once

#include <Windows.h>

#include <string>
#include <vector>

//! 変更履歴の置き場所（ノートフォルダーの中の隠しフォルダー）
constexpr const WCHAR* NOTE_HISTORY_DIR_NAME = L".履歴";
//! 1つのメモにつき残す上限。これを超えたら古いものから捨てる
constexpr int NOTE_HISTORY_MAX = 50;

//! サイドバーに出るメモか（＝履歴を残す相手か）
bool IsNoteFile( const std::wstring& strPath );

//! そのメモの履歴フォルダー（作りはしない。ノートでなければ空文字列）
std::wstring GetNoteHistoryDir( const std::wstring& strNotePath );

//! 上書きする直前の中身を履歴へ残す
/*!
	@param[in] strNotePath 対象のメモ（まだ古い中身が入っている状態で呼ぶ）
	@note 中身が直前の履歴と同じなら何もしない。失敗しても保存自体は止めない
*/
void SaveNoteHistory( const std::wstring& strNotePath );

//! 履歴の一覧（新しい順・フルパス）
std::vector<std::wstring> ListNoteHistory( const std::wstring& strNotePath );

//! メモの名前が変わったとき、履歴フォルダーも一緒に連れて行く
void RenameNoteHistory( const std::wstring& strOldPath, const std::wstring& strNewPath );

#endif /* SAKURA_NOTEHISTORY_H_ */
