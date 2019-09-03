// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <string>
#include <filesystem>
#include <memory>
#include <vector>
#include <map>
#include <deque>
#include <atomic>
#include <regex>
#include <condition_variable>
#include <functional>

#include "../core/basictypes.h"
#include "../core/Item.h"
#include "../core/FSReader.h"
#include "../core/MetaPath.h"

#include "LocalFSReader.h"
#include "FilterDlg.h"

class CFilesPPDlg;


class ItemListCtrl : public CMFCListCtrl
{
    DECLARE_DYNAMIC(ItemListCtrl);
    DECLARE_MESSAGE_MAP()
    void OnDropFiles(HDROP hDropInfo);
    CFilesPPDlg& dlg;

public:
    ItemListCtrl(CFilesPPDlg& d) : dlg(d) {}
    std::vector<Item*> filteredItems;
	void Sort(int iColumn, BOOL bAscending, BOOL bAdd ) override;
};

class Filter
{
public:
	bool noFolders;
	bool noFiles;
	bool filterFolders;
	std::regex re;
	int files = 0, folders = 0;
	int64_t size = 0;
	UserFeedback& userFeedback;

	Filter(const FilterDlg::Settings& filterSettings, UserFeedback& uf);
	void FilterNewItems(std::vector<std::unique_ptr<Item>>& items);
	void FilterNewItems(std::deque<std::unique_ptr<Item>>& items);
	void ApplyStatus();
};

class PathCtrl : public CEdit
{
	DECLARE_DYNAMIC(PathCtrl);
	DECLARE_MESSAGE_MAP()
	void OnDropFiles(HDROP hDropInfo);

	std::vector<std::shared_ptr<m::MegaApi>> megaAccounts;

	std::deque<MetaPath> historyBack, historyForward;

public:
	MetaPath metaPath;

	PathCtrl();
	void SetWindowText();
	void AcceptEdits();

	void PrepareNavigateBack();
	bool NavigateBack();
	bool NavigateForward();
};

class CFilesPPDlg : public CDialogEx, public UserFeedback
{
	DECLARE_DYNAMIC(CFilesPPDlg);

public:
	static std::atomic<unsigned> atomicInstances;
	static std::atomic<unsigned> atomicExitOk;
	static std::condition_variable shutdown_cv;

	CFilesPPDlg(CWnd* pParent = nullptr);	
	virtual ~CFilesPPDlg();

#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FILESPP_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	void LoadContent(bool resetFilter);
	LRESULT OnContentUpdate(WPARAM wParam, LPARAM lParam);
	void ContentEstablished();

	FilterDlg::Settings filterSettings;
	std::unique_ptr<Filter> filter;

	std::deque<std::unique_ptr<Item>> items;

	std::unique_ptr<FSReader> activeReader;

	CRect originatorWindowRect;

public:
	HICON m_hIcon;

	BOOL CanExit();

    void SetUserFeedbackCStr(const char* s) override;
    void ClearFilteredItems() override;
    void AddFilteredItem(Item*) override;
    void DoneAddingFilteredItems() override;
    void SetFilterText(const std::string& s) override;

	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnClose();
	virtual void OnOK();
	virtual void OnCancel();
	DECLARE_MESSAGE_MAP()
public:
	PathCtrl m_pathCtl;
	ItemListCtrl m_contentCtl;
	afx_msg void OnBnClickedButton1();
	afx_msg void OnNMDblclkList2(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnBnClickedButton2();
	CButton m_filterButton;
	afx_msg void OnLvnGetdispinfoList2(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDropFiles(HDROP hDropInfo);
	CStatusBarCtrl m_statusBar;
	afx_msg void OnNMRClickList2(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnBegindragList2(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnGetMinMaxInfo(MINMAXINFO* lpMMI);
	afx_msg void OnBnClickedButtonBack();
	afx_msg void OnBnClickedButtonForward();
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	CSpinButtonCtrl m_spinCtrl;
	afx_msg void OnDeltaposSpin1(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnLvnItemchangedList2(NMHDR *pNMHDR, LRESULT *pResult);
	afx_msg void OnDestroy();
};


class CFilesPPDlgThread : public CWinThread
{
public:
	CFilesPPDlg dlg;

	BOOL InitInstance() override;
};
