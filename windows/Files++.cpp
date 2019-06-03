// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "stdafx.h"
#include "Files++.h"
#include "Files++Dlg.h"
#include <thread>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


//wchar_t NewChar = __T('\x2018');

// CFilesPPApp

BEGIN_MESSAGE_MAP(CFilesPPApp, CWinApp)
	ON_COMMAND(ID_HELP, &CWinApp::OnHelp)
END_MESSAGE_MAP()


// CFilesPPApp construction

CFilesPPApp::CFilesPPApp()
{
	// support Restart Manager
	m_dwRestartManagerSupportFlags = AFX_RESTART_MANAGER_SUPPORT_RESTART;

	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}


// The one and only CFilesPPApp object

CFilesPPApp theApp;

// CFilesPPApp initialization

BOOL CFilesPPApp::InitInstance()
{
	//_crtBreakAlloc = 506;

	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinApp::InitInstance();


	// Initialize OLE libraries
	if (!AfxOleInit())
	{
		AfxMessageBox(IDP_OLE_INIT_FAILED);
		return FALSE;
	}

	AfxEnableControlContainer();

	// Create the shell manager, in case the dialog contains
	// any shell tree view or shell list view controls.
	CShellManager *pShellManager = new CShellManager;

	// Activate "Windows Native" visual manager for enabling themes in MFC controls
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerWindows));

	SetRegistryKey(_T("Files++"));
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);

	CFilesPPDlgThread* t = new CFilesPPDlgThread;
	t->m_bAutoDelete = true;
	BOOL b = t->CreateThread();
	if (!b) AfxMessageBox(_T("Thread creation failed"));

	std::mutex m;
	std::unique_lock g(m);
	CFilesPPDlg::shutdown_cv.wait(g, []() { return CFilesPPDlg::atomicExitOk > 0; });

	// Delete the shell manager created above.
	if (pShellManager != nullptr)
	{
		delete pShellManager;
	}

#if !defined(_AFXDLL) && !defined(_AFX_NO_MFC_CONTROLS_IN_DIALOGS)
	ControlBarCleanUp();
#endif

	// Since the dialog has been closed, return FALSE so that we exit the
	//  application, rather than start the application's message pump.
	return FALSE;
}

int CFilesPPApp::ExitInstance()
{
	AfxOleTerm(FALSE);

	return CWinApp::ExitInstance();
}
