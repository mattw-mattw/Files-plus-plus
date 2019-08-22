// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"		// main symbols
#include "../core/Item.h"

// CFilesPPApp:
// See FilesPP.cpp for the implementation of this class
//

class CFilesPPApp : public CWinApp
{
public:
	CFilesPPApp();

// Overrides
public:
	virtual BOOL InitInstance();
	virtual int ExitInstance();

// Implementation

	DECLARE_MESSAGE_MAP()
};

extern CFilesPPApp theApp;


UINT_PTR ExecMenu(CMenu& contextMenu, MenuActions& ma, UINT_PTR baseID, POINT xy, CWnd* wnd);
