/*! @file
	@brief 【自前改造】クイック退避（ドキュメント）の共通処理

	「名前を決めて・保存場所を決めて」が重いので、無題の文書は Ctrl+S で
	Google ドライブの退避フォルダーへ自動保存する。ここはその置き場所の決定と
	一覧取得をまとめたもの。詳細は MY_MODS.md を参照。

	SPDX-License-Identifier: Zlib
*/
#ifndef SAKURA_QUICKSTASH_H_
#define SAKURA_QUICKSTASH_H_
#pragma once

#include <Windows.h>

#include <string>
#include <vector>

//! ノートバーの並び順を覚えているファイルの名前
/*!
	退避フォルダーの中に置くが、メモではないので一覧には出さない。
	🔥 隠しファイル属性も付けているが、Google ドライブが別のPCへ配ったときに
	   属性が落ちることがあるので、名前でも弾く（二重の備え）。
*/
constexpr const WCHAR* NOTEBAR_ORDER_FILE_NAME = L".notebar-order.txt";

//! 退避フォルダーのパスを決めて、無ければ作る
/*!
	@return 退避フォルダーのフルパス。用意できなかったときは空文字列
	@note ドライブレターを決め打ちすると Drive 未マウント時に黙って死ぬので、
	      見つからなければユーザーフォルダーへ逃がす
*/
std::wstring GetQuickStashDir();

//! Google ドライブがまだ現れていない（接続待ち）か
/*!
	@return 待っている最中なら true
	@note この端末に Google ドライブが入っているのに候補ドライブが1つも見えない状態。
	      「メモが0件」ではなく「まだ来ていないだけ」と区別するために使う
*/
bool IsQuickStashDriveWaiting();

//! 退避したドキュメントを新しい順に取得する
/*!
	@param[in] nMax 取得する最大件数
	@return フルパスの配列（更新日時の新しい順）
*/
std::vector<std::wstring> GetQuickStashFiles( int nMax );

//! 指定したフォルダーの中身を新しい順に取得する
/*!
	@param[in] strDir 対象フォルダー（空なら何も返さない）
	@param[in] nMax   取得する最大件数
	@return フルパスの配列（更新日時の新しい順）
	@note ノートバー（改造⑪）と共用
*/
//! ファイル名の先頭の「YYYY-MM-DD_hhmm」を日時として読む
/*!
	@param[in]  pszFileName ファイル名（パスではなく名前だけ）
	@param[out] pftOut      読めたときの日時
	@return 読めたら true
*/
bool ParseStashNameTime( LPCWSTR pszFileName, FILETIME* pftOut );

std::vector<std::wstring> GetStashFilesInDir( const std::wstring& strDir, int nMax );

#endif /* SAKURA_QUICKSTASH_H_ */
