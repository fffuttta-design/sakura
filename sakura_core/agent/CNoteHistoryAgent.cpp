/*! @file */
/*
	【自前改造】メモの変更履歴を残す係

	SPDX-License-Identifier: Zlib
*/
#include "StdAfx.h"
#include "agent/CNoteHistoryAgent.h"

#include "doc/CEditDoc.h"
#include "util/notehistory.h"

/*! 上書きする直前に、いまディスクにある中身を履歴へ残す

	⚠ 履歴を残せなくても保存は止めない（保存できる方が大事）。
*/
ECallbackResult CNoteHistoryAgent::OnPreBeforeSave(SSaveInfo* pSaveInfo)
{
	if( nullptr != pSaveInfo ){
		const std::wstring strPath = pSaveInfo->cFilePath.c_str();
		if( IsNoteFile( strPath ) ){
			SaveNoteHistory( strPath );
		}
	}
	return CALLBACK_CONTINUE;
}
