
// FilesPPDlg.h : header file
//

#pragma once

#include <string>
#include <filesystem>
#include <memory>
#include <vector>
#include <map>
#include <deque>
#include <atomic>
#include <regex>
#include "FilterDlg.h"
#include <condition_variable>
#include <functional>

#include <megaapi.h>
namespace m = ::mega;

template<class T> struct copy_ptr : public std::unique_ptr<T>
{
	copy_ptr() {}
	explicit copy_ptr(T*) : unique_ptr(p) {}
	explicit copy_ptr(const copy_ptr& p) : unique_ptr(p ? p->copy() : nullptr) {}
	explicit copy_ptr(copy_ptr&& p) : unique_ptr(p.release()) {}
	void operator=(const copy_ptr& p) { reset(p ? p->copy() : nullptr); }
	void operator=(copy_ptr&& p) { reset(p.release()); }
	copy_ptr copy() const { return copy_ptr(*this); }
};

struct Item
{
	std::string u8Name;
	Item(std::string n) : u8Name(move(n)) {}
	Item() {}
	virtual ~Item() {}
	virtual bool isFolder() const { return true; }
	virtual int64_t size() const { return -1;  }
};

struct ItemLocalFS : public Item
{
	int64_t filesize;
	bool folder;
	ItemLocalFS(std::string n, int64_t s, bool f) : Item(move(n)), filesize(s), folder(f) {}
	bool isFolder() const override { return folder; }
	int64_t size() const override { return filesize; }
};

struct ItemError : public Item
{
	std::string message;
	ItemError(const std::string& s) : message(s) {}
};

struct ItemMegaAccount : public Item
{
	ItemMegaAccount(std::string n, std::shared_ptr<m::MegaApi> p) : Item(std::move(n)), masp(p) {}
	std::shared_ptr<m::MegaApi> masp;
};

struct ItemMegaNode : public Item
{
	ItemMegaNode(std::unique_ptr<m::MegaNode> n) : Item(n->getName()), mnode(std::move(n)) {}
	std::unique_ptr<m::MegaNode> mnode;
	bool isFolder() const override { return mnode->isFolder(); }
	int64_t size() const override { return isFolder() ? -1 : mnode->getSize(); }
};

struct ItemMegaInshare : public Item
{
	std::string insharepath(m::MegaNode& n, m::MegaApi& api)
	{
		std::unique_ptr<char[]> p(api.getNodePath(&n));
		return p ? std::string(p.get()) : n.getName();
	}

	ItemMegaInshare(std::unique_ptr<m::MegaNode> n, m::MegaApi& api, int64_t size, bool folder) : Item(insharepath(*n, api)), mnode(std::move(n)) {}
	std::unique_ptr<m::MegaNode> mnode;
};


class ItemListCtrl : public CMFCListCtrl
{
public:
	std::vector<Item*> filteredItems;
	void Sort(int iColumn, BOOL bAscending, BOOL bAdd ) override;
};


class CFilesPPDlg;


class FSReader
{
public:
	enum Action { 
		FILE_ACTION_APP_ERROR, FILE_ACTION_APP_NOTIFY_FAILURE, FILE_ACTION_APP_READCOMPLETE, FOLDER_RESOLVED_SOFTLINK,
		NEWITEM, DELETEDITEM, RENAMEDFROM, RENAMEDTO, INVALID_ACTION
	};

	FSReader(HWND targetwindow, bool r)
		: hwnd(targetwindow)
		, recurse(r)
	{
	}

	virtual ~FSReader() {}

	virtual void AddMenuItems(CMenu& m, int& id, std::map<int, std::function<void()>>& me) {}


protected:
	bool recurse = false;

	Action currentaction = INVALID_ACTION;
	std::vector<std::unique_ptr<Item>> currentitems;

	HWND hwnd;
	void Send();
	void Queue(Action action, std::unique_ptr<Item>&& p);
};

class MEGA
{
	std::mutex m;
	std::vector<std::shared_ptr<m::MegaApi>> megaAccounts;
	std::filesystem::path BasePath();

public:

	MEGA();

	std::vector<std::shared_ptr<m::MegaApi>> accounts() { std::lock_guard g(m); return megaAccounts; }

	std::pair<std::shared_ptr<m::MegaApi>, std::filesystem::path> makeTempAccount();
	void add(std::shared_ptr<m::MegaApi> p, std::filesystem::path path);

	void onLogin(const m::MegaError& e, const std::shared_ptr<m::MegaApi>& masp);
};
extern MEGA* g_mega;

class TopShelfReader : public FSReader
{
public:
	TopShelfReader(HWND targetwindow, bool r);
	~TopShelfReader();

	void AddMenuItems(CMenu& m, int& id, std::map<int, std::function<void()>>& me) override;

	void AddMEGAAccount();


private:

	void Threaded();

	// create thread last
	bool cancelling = false;
	std::thread workerthread;
};


struct OVERLAPPED0 : public OVERLAPPED
{
	OVERLAPPED0() { memset(this, 0, sizeof(*this));  }
};

class LocalFSReader : public FSReader
{
	HANDLE hDirectory = INVALID_HANDLE_VALUE;
	OVERLAPPED0 overlapped;
	std::string overlappedbuf;
	bool cancelling = false;

public:
	std::filesystem::path dir;

	LocalFSReader(std::filesystem::path p, HWND targetwindow, bool r);
	~LocalFSReader();

private:
	bool RequestChanges();
	bool ReadDir(const std::filesystem::path& p, bool recurse, const std::filesystem::path& recurseprefix);
	void Threaded();

	// create thread last
	std::thread workerthread;
};

class LocalVolumeReader : public FSReader
{
	OVERLAPPED0 overlapped;
	std::string overlappedbuf;
	bool cancelling = false;

public:
	LocalVolumeReader(HWND targetwindow, bool r);
	~LocalVolumeReader();

private:
	//bool RequestChanges();
	//bool Read(bool recurse, const std::filesystem::path& recurseprefix);
	void Threaded();

	// create thread last
	std::thread workerthread;
};

struct MegaAccountReader : public FSReader
{
	MegaAccountReader(std::shared_ptr<m::MegaApi> p, HWND targetwindow, bool r);
	~MegaAccountReader();

private:
	void Threaded();

	// create thread last
	std::shared_ptr<m::MegaApi> masp;
	bool cancelling = false;
	std::thread workerthread;
};

template<class T>
class NotifyQueue
{
	std::deque<T> queue;
	std::mutex m;
	std::condition_variable cv;
public:
	void push(T&& t)
	{
		std::lock_guard<std::mutex> g(m);
		queue.emplace_back(move(t));
		cv.notify_one();
	}
	bool pop(T& value)
	{
		std::unique_lock<mutex> g(m);
		cv.wait_for(g, 1s);
		if (queue.empty()) return false;
		value = std::move(queue.front());
		queue.pop_front();
		return true;
	}
};

struct NodeUpdateListener : public m::MegaGlobalListener
{
	NotifyQueue<std::unique_ptr<m::MegaNodeList>> nq;

	void onNodesUpdate(m::MegaApi*, m::MegaNodeList *nodes) override
	{
		nq.push(std::unique_ptr<m::MegaNodeList>(nodes->copy()));
	}
};


struct MegaFSReader : public FSReader
{
	MegaFSReader(std::shared_ptr<m::MegaApi> p, std::unique_ptr<m::MegaNode>, HWND targetwindow, bool r);
	~MegaFSReader();

private:
	//bool ReadDir(const std::filesystem::path& p, bool recurse, const std::filesystem::path& recurseprefix);
	void Threaded();

	// create thread last
	NodeUpdateListener listener;
	std::shared_ptr<m::MegaApi> masp;
	std::unique_ptr<m::MegaNode> mnode;
	bool cancelling = false;
	std::thread workerthread;
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
	CFilesPPDlg& dlg;

	Filter(const FilterDlg::Settings& filterSettings, CFilesPPDlg& d);
	void FilterNewItems(std::vector<std::unique_ptr<Item>>& items);
	void FilterNewItems(std::deque<std::unique_ptr<Item>>& items);
	void ApplyStatus();
};

class MetaPath
{
	enum PathType { None, TopShelf, LocalVolumes, LocalFS, MegaAccount, MegaFS };
	PathType pathType = None;
	std::filesystem::path localPath;
	std::shared_ptr<m::MegaApi> masp;
	copy_ptr<m::MegaNode> mnode;
public:
	bool operator==(const MetaPath& o) const;

	bool Ascend();
	bool Descend(const Item&);

	void SetLocalPath(const std::filesystem::path& p);
	bool GetLocalPath(std::filesystem::path& p) const;

	std::unique_ptr<FSReader> GetContentReader(HWND wnd, bool recurse) const;

	std::string u8DisplayPath() const;

	std::function<bool(std::unique_ptr<Item>& a, std::unique_ptr<Item>& b)> nodeCompare();
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


class Favourites
{
	std::vector<MetaPath> favourites;
	std::mutex m;
public:

	std::vector<MetaPath> copy()
	{
		std::lock_guard g(m);
		return favourites;
	}

	bool Toggle(const MetaPath& p)
	{
		std::lock_guard g(m);
		for (unsigned i = 0; i < favourites.size(); ++i)
		{
			if (favourites[i].u8DisplayPath() == p.u8DisplayPath())
			{
				favourites.erase(favourites.begin() + i);
				return false;
			}
		}
		favourites.push_back(p);
		return true;
	}
};

extern Favourites g_favourites;

// CFilesPPDlg dialog
class CFilesPPDlg : public CDialogEx
{
	DECLARE_DYNAMIC(CFilesPPDlg);

// Construction
public:
	static std::atomic<unsigned> atomicInstances;
	static std::condition_variable shutdown_cv;

	CFilesPPDlg(CWnd* pParent = nullptr);	// standard constructor
	virtual ~CFilesPPDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FILESPP_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support

	void LoadContent(bool reestFilter);
	LRESULT OnContentUpdate(WPARAM wParam, LPARAM lParam);
	void ContentEstablished();

	FilterDlg::Settings filterSettings;
	std::unique_ptr<Filter> filter;

	std::deque<std::unique_ptr<Item>> items;

	std::unique_ptr<FSReader> activeReader;

	CRect originatorWindowRect;

// Implementation
public:
	HICON m_hIcon;

	BOOL CanExit();

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
};


class CFilesPPDlgThread : public CWinThread
{
public:
	CFilesPPDlg dlg;

	// run dialog in here
	BOOL InitInstance() override;
};
