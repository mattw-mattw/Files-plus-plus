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

	std::vector<ApiPtr> megaAccounts;

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

template<class T1, class T2>
void match_removeintersection(T1& a, T2& b, std::function<bool(typename T1::value_type&, typename T2::value_type&)> cmpless)
{
	if (a.empty() || b.empty()) return;
	sort(a.begin(), a.end(), cmpless);
	sort(b.begin(), b.end(), cmpless);

	T1::iterator ra = std::lower_bound(a.begin(), a.end(), *b.begin()), wa = ra;
	T2::iterator rb = std::lower_bound(b.begin(), b.end(), *a.begin());
	T1::iterator final_a = upper_bound(a.begin(), a.end(), *(b.end()-1));
	T2::iterator final_b = lower_bound(b.begin(), b.end(), *(a.end()-1));
	bool writinga = false;
	while (ra != final_a && ra != a.end() && rb != final_b && rb != b.end())
	{
		if (cmpless(*ra, *rb))
		{
			if (writinga) *wa = move(*ra);
			++wa, ++ra;
		}
		else if (cmpless(*rb, *ra))
		{
			++rb;
		}
		else
		{
			// if we are removing a directory, then also remove anything under it (eg. when a folder is dragged to the rubbish, we don't get notified for the contents - though we are for real deletion (in reverse depth-first))
			if ((*ra)->isFolder())
			{
				string foldername = (*ra)->u8Name;
				if ((*ra)->u8Name[(*ra)->u8Name.size() - 1] != '\\') foldername.append(1, '\\');
				++ra, ++rb;
				while (ra != a.end() && 0 == strncmp((*ra)->u8Name.c_str(), foldername.c_str(), foldername.size()))
					++ra;
			}
			else
			{
				++ra, ++rb;
			}
			writinga = true;
		}
	}
	if (writinga)
	{
		while (ra != a.end())
		{
			*(wa++) = move(*(ra++));
		}
		a.erase(wa, a.end());
	}
}

template<class T>
class SortedAndAdditional
{
	std::deque<T> sortedItems;
	std::deque<T> additionalItems;
	std::function<bool(const T&, const T&)> cmpless;

public:

	SortedAndAdditional(std::function<bool(const T&, const T&)> less) : cmpless(less) {}

	void for_each(std::function<void(T& i)> f)
	{
		for (auto& i : sortedItems) f(i);
		for (auto& i : additionalItems) f(i);
	}

	void removeintersection(std::vector<T>& b, bool stable)
	{
		std::sort(b.begin(), b.end(), cmpless);
		match_removeintersection(sortedItems, b, cmpless);
		if (!stable)
		{
			std::sort(additionalItems.begin(), additionalItems.end(), cmpless);
			match_removeintersection(additionalItems, b, cmpless);
		}
		else
		{
			for (auto i = additionalItems.begin(); i != additionalItems.end(); )
			{
				if (std::binary_search(b.begin(), b.end(), *i, cmpless)) i = additionalItems.erase(i);
				else ++i;
			}
		}
	}

	size_t size()
	{
		return sortedItems.size() + additionalItems.size();
	}

	void clear()
	{
		sortedItems.clear();
		additionalItems.clear();
	}

	void add(T&& t)
	{
		additionalItems.emplace_back(move(t));
	}

	T& operator[](size_t n)
	{
		auto s = sortedItems.size();
		return n < s ? sortedItems[n] : additionalItems[n - s];
	}

	void sort(std::function<bool(const T&, const T&)> newless)
	{
		if (newless)
		{
			cmpless = newless;
			for (auto& i : additionalItems) sortedItems.emplace_back(move(i));
			additionalItems.clear();
			std::sort(sortedItems.begin(), sortedItems.end(), cmpless);
		}
		else
		{
			std::sort(additionalItems.begin(), additionalItems.end(), cmpless);
			auto n = additionalItems.size();
			for (auto& i : additionalItems) sortedItems.emplace_back(move(i));
			additionalItems.clear();
			std::inplace_merge(sortedItems.begin(), sortedItems.end() - n, sortedItems.end());
		}
	}
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

	SortedAndAdditional<std::unique_ptr<Item>> items;
	SortedAndAdditional<Item*> filteredItems;
	friend class ItemListCtrl;

	std::unique_ptr<FSReader> activeReader;

	CRect originatorWindowRect;

	std::unique_ptr<FilterDlg> modelessFilter;

public:
	HICON m_hIcon;

	BOOL CanExit();

    void SavePlaylist();

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
