// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "stdafx.h"
#include "Files++.h"
#include "FilterDlg.h"
#include "afxdialogex.h"


// FilterDlg dialog

IMPLEMENT_DYNAMIC(FilterDlg, CDialogEx)

FilterDlg::FilterDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_FILTERDIALOG, pParent)
{
#ifndef _WIN32_WCE
	EnableActiveAccessibility();
#endif
}

FilterDlg::~FilterDlg()
{
}

BOOL FilterDlg::OnInitDialog()
{
	BOOL b = CDialogEx::OnInitDialog();

	m_filtertypeCtl.InsertString(0, _T("Filter files"));
	m_filtertypeCtl.InsertString(1, _T("Filter all"));
	m_filtertypeCtl.InsertString(2, _T("Hide folders"));
	m_filtertypeCtl.InsertString(3, _T("Hide files"));
	m_filtertypeCtl.SelectString(-1, m_settings.filtertype);
	return b;
}

void FilterDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Text(pDX, IDC_EDIT1, m_settings.text);
	DDX_CBString(pDX, IDC_FILTERTYPE, m_settings.filtertype);
	DDX_Check(pDX, IDC_CHECKREGEX, m_settings.regularexpression);
	DDX_Check(pDX, IDC_CHECKCASE, m_settings.casesensitive);
	DDX_Check(pDX, IDC_CHECKRECURSE, m_settings.recursesubfolders);
	DDX_Check(pDX, IDC_CHECKRESET, m_settings.resetonnavigate);
	DDX_Control(pDX, IDC_FILTERTYPE, m_filtertypeCtl);
}

BEGIN_MESSAGE_MAP(FilterDlg, CDialogEx)
	ON_BN_CLICKED(IDOK2, &FilterDlg::OnBnClickedOk2)
	ON_BN_CLICKED(IDOK, &FilterDlg::OnBnClickedOk)
	ON_BN_CLICKED(IDAPPLY, &FilterDlg::OnBnClickedApply)
	ON_BN_CLICKED(IDCANCEL, &FilterDlg::OnBnClickedCancel)
END_MESSAGE_MAP()

void FilterDlg::OnBnClickedOk2()
{
	m_settings.Reset();
	EndDialog(IDOK);
}
void FilterDlg::OnBnClickedOk()
{
	UpdateData(TRUE);
	onOkAction();
	DestroyWindow();
}
void FilterDlg::OnBnClickedApply()
{
	UpdateData(TRUE);
	onOkAction();
	SetDefID(IDAPPLY);
}
void FilterDlg::OnBnClickedCancel()
{
	DestroyWindow();
}
