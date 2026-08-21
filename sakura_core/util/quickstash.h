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

#include <string>
#include <vector>

//! 退避フォルダーのパスを決めて、無ければ作る
/*!
	@return 退避フォルダーのフルパス。用意できなかったときは空文字列
	@note ドライブレターを決め打ちすると Drive 未マウント時に黙って死ぬので、
	      見つからなければユーザーフォルダーへ逃がす
*/
std::wstring GetQuickStashDir();

//! 退避したドキュメントを新しい順に取得する
/*!
	@param[in] nMax 取得する最大件数
	@return フルパスの配列（更新日時の新しい順）
*/
std::vector<std::wstring> GetQuickStashFiles( int nMax );

#endif /* SAKURA_QUICKSTASH_H_ */
