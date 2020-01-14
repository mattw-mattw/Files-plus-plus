// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

// FilterDlg dialog

class FilterDlg : public CDialogEx
{
	DECLARE_DYNAMIC(FilterDlg)

public:
	FilterDlg(CWnd* pParent = nullptr);   // standard constructor
	virtual ~FilterDlg();

	BOOL OnInitDialog() override;
// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FILTERDIALOG };
#endif

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

	DECLARE_MESSAGE_MAP()
public:

	struct Settings
	{
		CString filtertype = _T("Filter files");
		CString text = _T("");
		BOOL casesensitive = false;
		BOOL regularexpression = true;
		BOOL recursesubfolders = false;
		BOOL resetonnavigate = true;

		void Reset()
		{
			filtertype = _T("Filter files");
			text = _T("");
			recursesubfolders = FALSE;
			resetonnavigate = TRUE;
			// case sensitive, and regular expression, are persisted anyway
		}
	} m_settings;

	afx_msg void OnBnClickedOk2();
	afx_msg void OnBnClickedOk();
	afx_msg void OnBnClickedApply();
	afx_msg void OnBnClickedCancel();
	CComboBox m_filtertypeCtl;

	std::function<void()> onOkAction;
};
