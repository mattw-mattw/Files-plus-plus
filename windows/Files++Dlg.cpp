// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "stdafx.h"
#include "Files++.h"
#include "Files++Dlg.h"
#include "afxdialogex.h"
#include "FilterDlg.h"
#include "FilterDlg.h"
#include "resource.h"

#include <chrono>
#include <thread>
#include <algorithm> 
#include <locale>
#include <shlobj_core.h>
#include <fstream>


#undef min
#undef max

using namespace std;
namespace fs = std::filesystem;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

MEGA* g_mega = nullptr;

string StartPath()
{
#pragma warning(disable: 4996)
	const char* s = getenv("USERPROFILE");
	return s ? s : "c:\\";
}

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
	EnableActiveAccessibility();
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


void ItemListCtrl::Sort(int iColumn, BOOL bAscending, BOOL bAdd)
{

	auto sortFunction = [this, iColumn, bAscending](const Item* a, const Item* b) {
		return iColumn == 0
			? (bAscending ? a->u8Name > b->u8Name : a->u8Name < b->u8Name)
			: (bAscending ? a->size() > b->size() : a->size() < b->size()); };

	if (bAdd)
		std::stable_sort(filteredItems.begin(), filteredItems.end(), sortFunction);
	else
		std::sort(filteredItems.begin(), filteredItems.end(), sortFunction);

	GetHeaderCtrl().SetSortColumn(iColumn, bAscending, bAdd);

	m_iSortedColumn = iColumn;
	m_bAscending = bAscending;

	InvalidateRect(NULL);
}



// CFilesPPDlg dialog


IMPLEMENT_DYNAMIC(CFilesPPDlg, CDialogEx);

CFilesPPDlg::CFilesPPDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_FILESPP_DIALOG, pParent)
{
	if (!g_mega) g_mega = new MEGA;

	EnableActiveAccessibility();
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
	++atomicInstances;
}

CFilesPPDlg::~CFilesPPDlg()
{
	--atomicInstances;
	shutdown_cv.notify_all();
}

std::atomic<unsigned> CFilesPPDlg::atomicInstances;
std::condition_variable CFilesPPDlg::shutdown_cv;

BOOL CFilesPPDlgThread::InitInstance() 
{
	SCODE sc = ::OleInitialize(NULL);
	assert(!FAILED(sc));
	dlg.DoModal();
	return FALSE;
}

void CFilesPPDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_EDIT1, m_pathCtl);
	DDX_Control(pDX, IDC_LIST2, m_contentCtl);
	DDX_Control(pDX, IDC_BUTTON2, m_filterButton);
	DDX_Control(pDX, IDC_SPIN1, m_spinCtrl);
}

#define WM_APP_CONTENTUPDATE (WM_APP+1)

BEGIN_MESSAGE_MAP(CFilesPPDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_CLOSE()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_BUTTON1, &CFilesPPDlg::OnBnClickedButton1)
	ON_NOTIFY(NM_DBLCLK, IDC_LIST2, &CFilesPPDlg::OnNMDblclkList2)
	ON_BN_CLICKED(IDC_BUTTON2, &CFilesPPDlg::OnBnClickedButton2)
	ON_NOTIFY(LVN_GETDISPINFO, IDC_LIST2, &CFilesPPDlg::OnLvnGetdispinfoList2)
	ON_WM_DROPFILES()
	ON_NOTIFY(NM_RCLICK, IDC_LIST2, &CFilesPPDlg::OnNMRClickList2)
	ON_NOTIFY(LVN_BEGINDRAG, IDC_LIST2, &CFilesPPDlg::OnLvnBegindragList2)
	ON_MESSAGE(WM_APP_CONTENTUPDATE, &CFilesPPDlg::OnContentUpdate)
	ON_WM_GETMINMAXINFO()
	ON_BN_CLICKED(IDC_BUTTON_BACK, &CFilesPPDlg::OnBnClickedButtonBack)
	ON_BN_CLICKED(IDC_BUTTON_FORWARD, &CFilesPPDlg::OnBnClickedButtonForward)
	ON_WM_RBUTTONDOWN()
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN1, &CFilesPPDlg::OnDeltaposSpin1)
	ON_NOTIFY(LVN_ITEMCHANGED, IDC_LIST2, &CFilesPPDlg::OnLvnItemchangedList2)
END_MESSAGE_MAP()


BEGIN_MESSAGE_MAP(PathCtrl, CEdit)
	ON_WM_DROPFILES()
END_MESSAGE_MAP()

IMPLEMENT_DYNAMIC(PathCtrl, CEdit);

void PathCtrl::OnDropFiles(HDROP hDropInfo)
{
	int nCntFiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, 0, 0);

	for (int j = 0; j < nCntFiles; j++) 
	{
		UINT bufsize = ::DragQueryFileW(hDropInfo, j, NULL, 0) + 1;
		unique_ptr<wchar_t[]> buf(new wchar_t[bufsize]);

		if (::DragQueryFileW(hDropInfo, j, buf.get(), bufsize))
		{
			buf[bufsize - 1] = 0;
			fs::path p(buf.get());
			error_code e;
			auto s = fs::status(p, e);
			if (!e && fs::is_regular_file(s))
			{
				::SetWindowTextW(m_hWnd, p.parent_path().wstring().c_str());
				SetFocus();
				static_cast<CFilesPPDlg*>(GetParent())->OnOK();
				break;
			}
			else if (!e && fs::is_directory(s))
			{
				::SetWindowTextW(m_hWnd, buf.get());
				SetFocus();
				static_cast<CFilesPPDlg*>(GetParent())->OnOK();
				break;
			}
		}
	}
}

PathCtrl::PathCtrl()
{
	metaPath.SetLocalPath(StartPath());
}

void PathCtrl::SetWindowText()
{
	::SetWindowTextA(m_hWnd, metaPath.u8DisplayPath().c_str());
}

void PathCtrl::AcceptEdits()
{
	CString csPath;
	GetWindowText(csPath);
	metaPath.SetLocalPath(fs::u8path(string(CT2CA(csPath))));
}

void PathCtrl::PrepareNavigateBack()
{
	if (!historyBack.empty() && historyBack.back() == metaPath) return;
	historyBack.push_back(metaPath);
	historyForward.clear();
}

bool PathCtrl::NavigateBack()
{
	if (historyBack.empty()) return false;
	historyForward.push_back(metaPath);
	metaPath = historyBack.back();
	historyBack.pop_back();
	return true;
}

bool PathCtrl::NavigateForward()
{
	if (historyForward.empty()) return false;
	historyBack.push_back(metaPath);
	metaPath = historyForward.back();
	historyForward.pop_back();
	return true;
}

// CFilesPPDlg message handlers
BOOL CFilesPPDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	m_contentCtl.SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
	m_contentCtl.EnableMarkSortedColumn();
	m_contentCtl.EnableMultipleSort(true);
	m_contentCtl.InsertColumn(0, _T("Name"), LVCFMT_LEFT, 200, 0);
	m_contentCtl.InsertColumn(1, _T("Size"), LVCFMT_RIGHT, 100, 1);
	m_contentCtl.InsertColumn(2, _T(""), LVCFMT_RIGHT, 10, 2);  // nothing shown for this one - makes autosizing work better

	if (auto statBarPlaceholder = GetDlgItem(IDC_STATUSBARPLACEHOLDER))
	{
		CRect r;
		statBarPlaceholder->GetWindowRect(&r);
		statBarPlaceholder->DestroyWindow();

		m_statusBar.Create(WS_CHILD | WS_BORDER | WS_VISIBLE, r, this, IDC_STATUSBARPLACEHOLDER);
		m_statusBar.SetMinHeight(r.Height());
		m_statusBar.SetSimple(true);
		GetDynamicLayout()->AddItem(IDC_STATUSBARPLACEHOLDER, CMFCDynamicLayout::MoveVertical(100), CMFCDynamicLayout::SizeHorizontal(100));
	}

	if (originatorWindowRect.Height() && originatorWindowRect.Width())
	{
		CRect r = originatorWindowRect;
		r.MoveToX(originatorWindowRect.right);
		MoveWindow(r);
	}

	LoadContent(true);

	m_spinCtrl.SetRange(0, 2);
	m_spinCtrl.SetPos(1);


	return TRUE;  // return TRUE  unless you set the focus to a control
}



MegaAccountReader::MegaAccountReader(std::shared_ptr<m::MegaApi> p, QueueTrigger t, bool r)
	: FSReader(t, r)
	, masp(p)
	, workerthread([this]() { Threaded(); })
{
}

MegaAccountReader::~MegaAccountReader()
{
	cancelling = true;
	workerthread.join();
}

void MegaAccountReader::Threaded()
{
	unique_ptr<m::MegaNode> root(masp->getRootNode());
	unique_ptr<m::MegaNode> inbox(masp->getInboxNode());
	unique_ptr<m::MegaNode> rubbish(masp->getRubbishNode());

	Queue(NEWITEM, make_unique<ItemMegaNode>(move(root)));
	Queue(NEWITEM, make_unique<ItemMegaNode>(move(inbox)));
	Queue(NEWITEM, make_unique<ItemMegaNode>(move(rubbish)));

	unique_ptr<m::MegaNodeList> inshares(masp->getInShares());
	if (inshares)
	{
		for (int i = 0; i < inshares->size(); ++i)
		{
			Queue(NEWITEM, make_unique<ItemMegaInshare>(std::unique_ptr<m::MegaNode>(inshares->get(i)->copy()), *masp, -1, true));
		}
	}

	Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
	Send();
}

MegaFSReader::MegaFSReader(std::shared_ptr<m::MegaApi> p, std::unique_ptr<m::MegaNode> n, QueueTrigger t, bool r)
	: FSReader(t, r)
	, masp(p)
	, mnode(move(n))
	, workerthread([this]() { Threaded(); })
{
}

MegaFSReader::~MegaFSReader()
{
	cancelling = true;
	listener.nq.push(nullptr);
	workerthread.join();
}

void MegaFSReader::Threaded()
{
	masp->addGlobalListener(&listener);

	unique_ptr<m::MegaChildrenLists> mc(masp->getFileFolderChildren(mnode.get()));
	if (mc)
	{
		m::MegaNodeList* folders = mc->getFolderList();
		m::MegaNodeList* files = mc->getFileList();

		for (int i = 0; i < folders->size(); ++i)
		{
			Queue(NEWITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(folders->get(i)->copy())));
		}
		for (int i = 0; i < files->size(); ++i)
		{
			Queue(NEWITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(files->get(i)->copy())));
		}
	}
	Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
	Send();

	while (!cancelling)
	{
		unique_ptr<m::MegaNodeList> nodes;
		if (listener.nq.pop(nodes) && nodes)
		{
			for (int i = 0; i < nodes->size(); ++i)
			{
				auto n = nodes->get(i);
				if (n->getParentHandle() == mnode->getHandle() && n->isFolder() || n->isFile())
				{
					if (n->hasChanged(m::MegaNode::CHANGE_TYPE_REMOVED))
					{
						Queue(DELETEDITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(n->copy())));
					}
					else if (n->hasChanged(m::MegaNode::CHANGE_TYPE_NEW))
					{
						Queue(NEWITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(n->copy())));
					}
					else if (n->hasChanged(m::MegaNode::CHANGE_TYPE_ATTRIBUTES)) // could be renamed
					{
						Queue(DELETEDITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(n->copy())));
						Queue(NEWITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(n->copy())));
					}
				}
			}
			Send();
		}
	}
	masp->removeGlobalListener(&listener);
}


void CFilesPPDlg::LoadContent(bool resetFilter)
{
	m_pathCtl.SetWindowText();
	m_contentCtl.SetItemCount(0);
	m_contentCtl.filteredItems.clear();
	items.clear();

	if (resetFilter)
	{
		if (filterSettings.resetonnavigate) filterSettings.Reset();
		filterSettings.recursesubfolders = false;
	}
	filter.reset(new Filter(filterSettings, *this));

	FSReader::QueueTrigger t = [wnd = m_hWnd]() { ::PostMessage(wnd, WM_APP_CONTENTUPDATE, (WPARAM)0, (LPARAM)0); };

	activeReader = m_pathCtl.metaPath.GetContentReader(t, filterSettings.recursesubfolders);
}


template<class T1, class T2>
void sort_match_removeintersection(T1& a, T2& b, std::function<bool(typename T1::value_type&, typename T2::value_type&)> cmpless)
{
	sort(a.begin(), a.end(), cmpless);
	sort(b.begin(), b.end(), cmpless);

	T1::iterator ra = a.begin(), wa = a.begin();
	T2::iterator rb = b.begin();
	bool writinga = false;
	while (ra != a.end() && rb != b.end())
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

template<class T1, class T2>
void remove_if_absent(T1& a, T2& b)
{
	sort(b.begin(), b.end());// , [cmpfield](T2::value_type& b1, T2::value_type& b2) { return cmpfield(b1) < cmpfield(b2); });
	T1::iterator ra = a.begin(), wa = a.begin();
	bool writinga = false;
	while (ra != a.end())
	{
		unique_ptr<Item> hackptr(*ra);
		if (binary_search(b.begin(), b.end(), hackptr))
		{
			if (writinga) *wa = move(*ra);
			++wa, ++ra;
		}
		else
		{
			++ra;
			writinga = true;
		}
		hackptr.release();
	}
	a.erase(wa, a.end());
}

std::function<bool(unique_ptr<Item>& a, unique_ptr<Item>& b)> MetaPath::nodeCompare()
{
	switch (pathType)
	{
	case MegaFS: return [](unique_ptr<Item>& a, unique_ptr<Item>& b) 
							{ 
								return static_cast<ItemMegaNode*>(a.get())->mnode->getHandle() < static_cast<ItemMegaNode*>(b.get())->mnode->getHandle();
							};
	default: return [](unique_ptr<Item>& a, unique_ptr<Item>& b) 
							{ 
								return a->u8Name < b->u8Name; 
							};
	}
}

LRESULT CFilesPPDlg::OnContentUpdate(WPARAM wParam, LPARAM lParam)
{
	
	FSReader::Entry entry;
	while (activeReader->q.pop(entry))
	{
		switch (entry.action)
		{
		case FSReader::NEWITEM:
		case FSReader::RENAMEDTO:
			if (filter) filter->FilterNewItems(entry.batch);

			for (auto& i : entry.batch) items.emplace_back(move(i));
			break;

		case FSReader::DELETEDITEM:
		case FSReader::RENAMEDFROM:
			sort_match_removeintersection(items, entry.batch, m_pathCtl.metaPath.nodeCompare());
			remove_if_absent(m_contentCtl.filteredItems, items);
			m_contentCtl.SetItemCount(m_contentCtl.filteredItems.size());
			break;

		case FSReader::FILE_ACTION_APP_ERROR:
			AfxMessageBox(_T("The directory could not be read"));
			break;

		case FSReader::FILE_ACTION_APP_NOTIFY_FAILURE:
			AfxMessageBox(_T("Notifications are not working, file changes will not be reflected"));
			break;

		case FSReader::FILE_ACTION_APP_READCOMPLETE:
			ContentEstablished();
			break;

		case FSReader::FOLDER_RESOLVED_SOFTLINK:
			if (auto p = dynamic_cast<LocalFSReader*>(activeReader.get()))
			{
				m_pathCtl.metaPath.SetLocalPath(p->dir);
				m_pathCtl.SetWindowText();
			}
			break;
		}

	}
	
	return 0;
}


void CFilesPPDlg::ContentEstablished()
{
	int fullwidth = 0;
	for (int i = 0; i < m_contentCtl.GetHeaderCtrl().GetItemCount(); ++i)
	{
		m_contentCtl.SetColumnWidth(i, LVSCW_AUTOSIZE_USEHEADER);
		if (i+1 < m_contentCtl.GetHeaderCtrl().GetItemCount()) fullwidth += m_contentCtl.GetColumnWidth(i);
	}
	
	CRect r1, r2;
	GetWindowRect(r1);
	GetClientRect(r2);
	r1.right = r1.left + fullwidth + r1.Width() - r2.Width() + GetSystemMetrics(SM_CXVSCROLL) + 10;
	MoveWindow(r1);
	m_contentCtl.SetColumnWidth(m_contentCtl.GetHeaderCtrl().GetItemCount()-1, LVSCW_AUTOSIZE_USEHEADER);
}

void CFilesPPDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CFilesPPDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CFilesPPDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

// Automation servers should not exit when a user closes the UI
//  if a controller still holds on to one of its objects.  These
//  message handlers make sure that if the proxy is still in use,
//  then the UI is hidden but the dialog remains around if it
//  is dismissed.

void CFilesPPDlg::OnClose()
{
	if (CanExit())
		CDialogEx::OnCancel();
}

void CFilesPPDlg::OnOK()
{
	//if (CanExit())
	//	CDialogEx::OnOK();

	CWnd* pwndCtrl = GetFocus();
	if (pwndCtrl && pwndCtrl->GetDlgCtrlID() == IDC_EDIT1)
	{
		m_pathCtl.PrepareNavigateBack();
		m_pathCtl.AcceptEdits();
		LoadContent(false);
	}


}

void CFilesPPDlg::OnCancel()
{
	//if (CanExit())
	//	CDialogEx::OnCancel();
}

BOOL CFilesPPDlg::CanExit()
{
	return TRUE;
}

void CFilesPPDlg::OnBnClickedButton1()
{
	auto tmp = m_pathCtl.metaPath;
	if (tmp.Ascend())
	{
		m_pathCtl.metaPath = tmp;
		m_pathCtl.PrepareNavigateBack();
		LoadContent(true);
	}
}

void CFilesPPDlg::OnNMDblclkList2(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	if (pNMItemActivate->iItem >= 0 && pNMItemActivate->iItem < m_contentCtl.GetItemCount() && (unsigned)pNMItemActivate->iItem < m_contentCtl.filteredItems.size())
	{
		Item& item = *m_contentCtl.filteredItems[pNMItemActivate->iItem];

		//CString s = m_contentCtl.GetItemText(pNMItemActivate->iItem, 0);

		//// remove trailing \ for directories
		//if (s.GetLength() > 0 && s[s.GetLength() - 1] == _T('\\'))
		//{
		//	s.Delete(s.GetLength() - 1, 1);
		//}

		if (pNMItemActivate->uKeyFlags & LVKF_SHIFT)
		{
			// make a new top level window (complete with its own gui thread) to load subdir 

			auto t = make_unique<CFilesPPDlgThread>();

			fs::path p;
			if (m_pathCtl.metaPath.GetLocalPath(p))
			{
				t->dlg.m_pathCtl.metaPath.SetLocalPath(p);
				if (t->dlg.m_pathCtl.metaPath.Descend(item))//string(CT2CA(s))))
				{
					t->m_bAutoDelete = true;
					t->dlg.filterSettings = filterSettings;
					GetWindowRect(t->dlg.originatorWindowRect);
					BOOL b = t->CreateThread();
					if (!b) AfxMessageBox(_T("Thread creation failed"));
					else t.release();
				}
			}
		}
		else
		{
			m_pathCtl.PrepareNavigateBack();
			if (m_pathCtl.metaPath.Descend(item)) // string(CT2CA(s))))
			{
				// load subdir in this window
				LoadContent(true);
			}
		}
	}
	*pResult = 0;
}


void CFilesPPDlg::OnBnClickedButton2()
{
	FilterDlg dlg;
	dlg.m_settings = dlg.m_settings = filterSettings;

	switch (dlg.DoModal())
	{
	case IDOK: 
		if (filterSettings.recursesubfolders != dlg.m_settings.recursesubfolders)
		{
			filterSettings = dlg.m_settings;
			LoadContent(false);
		}
		else
		{
			filterSettings = dlg.m_settings;
			filter.reset(new Filter(filterSettings, *this));
			filter->FilterNewItems(items);
		}
		break;
	case IDCANCEL: 
		break;
	}
}

Filter::Filter(const FilterDlg::Settings& filterSettings, CFilesPPDlg& d) : dlg(d)
{
	noFolders = filterSettings.filtertype == "Hide folders";
	noFiles = filterSettings.filtertype == "Hide files";
	filterFolders = filterSettings.filtertype == "Filter all" || noFiles;

	CString buttonText;
	if (noFolders) buttonText = "Files: ";
	if (noFiles) buttonText = "Folders: ";
	buttonText += filterSettings.text.IsEmpty() ? _T(".*") : filterSettings.text;
	dlg.m_filterButton.SetWindowText(buttonText);

	string exp = CT2CA(filterSettings.text);
	if (!filterSettings.regularexpression)
	{
		// convert non-re to re
		for (int i = exp.size(); i--; )
		{
			if (exp[i] == '?') exp[i] = '.';
			else if (exp[i] == '*') exp.replace(i, 1, ".*");
			else exp.insert(i, "\\");
		}
	}

	re = std::regex(exp.begin(), exp.end(), filterSettings.casesensitive ? std::regex::ECMAScript : std::regex::icase);

	dlg.m_contentCtl.filteredItems.clear();
}

void Filter::FilterNewItems(std::deque<std::unique_ptr<Item>>& items)
{
	for (auto& p : items)
	{
		if (p->isFolder() && noFolders) continue;
		if (!p->isFolder() && noFiles) continue;
		if (!p->isFolder() || filterFolders)
		{
			if (!regex_search(p->u8Name.c_str(), re)) continue;
		}
		dlg.m_contentCtl.filteredItems.push_back(p.get());
		if (p->isFolder()) ++folders;
		else ++files, size += p->size();
	}
	dlg.m_contentCtl.SetItemCount(dlg.m_contentCtl.filteredItems.size());
}

void Filter::FilterNewItems(std::vector<std::unique_ptr<Item>>& items)
{
	for (auto& p : items)
	{
		if (p->isFolder() && noFolders) continue;
		if (!p->isFolder() && noFiles) continue;
		if (!p->isFolder() || filterFolders)
		{
			if (!regex_search(p->u8Name.c_str(), re)) continue;
		}
		dlg.m_contentCtl.filteredItems.push_back(p.get());
		if (p->isFolder()) ++folders;
		else ++files, size += p->size();
	}
	dlg.m_contentCtl.SetItemCount(dlg.m_contentCtl.filteredItems.size());
}

void Filter::ApplyStatus()
{
	CString s;
	s.Format(_T("Folders: %d  Files: %d  Size: %lld"), folders, files, size);
	dlg.m_statusBar.SetText(s, 255, SBT_NOBORDERS);
}


void CFilesPPDlg::OnLvnGetdispinfoList2(NMHDR *pNMHDR, LRESULT *pResult)
{
	NMLVDISPINFO *pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
	LV_ITEM* pItem = &(pDispInfo)->item;

	unsigned iItem = (unsigned)pItem->iItem;

	if (iItem < m_contentCtl.filteredItems.size() && (pItem->mask & LVIF_TEXT)) //valid text buffer?
	{
		switch (pItem->iSubItem)
		{
		case 0: //fill in main text
			wcscpy_s(pItem->pszText, pItem->cchTextMax, CString(CA2CT(m_contentCtl.filteredItems[iItem]->u8Name.c_str())) + (m_contentCtl.filteredItems[iItem]->isFolder() ? _T("\\") : _T("")));
			break;
		case 1: //fill in sub item 1 text
			if (!m_contentCtl.filteredItems[iItem]->isFolder() || m_contentCtl.filteredItems[iItem]->size() >= 0)
				wcscpy_s(pItem->pszText, pItem->cchTextMax, CString(CA2CT(to_string(m_contentCtl.filteredItems[iItem]->size()).c_str())));
			break;
		}
	}
	*pResult = 0;
}


void CFilesPPDlg::OnDropFiles(HDROP hDropInfo)
{
	// TODO: Add your message handler code here and/or call default

	CDialogEx::OnDropFiles(hDropInfo);
}

enum { MENU_COPYNAMES = 1, MENU_COPYPATHS, MENU_COPYNAMESQUOTED, MENU_COPYPATHSQUOTED, MENU_EXPLORETO, MENU_EXPLOREPROPERTIES, MENU_SUBSEQUENT };

void ExploreTo(HWND hwnd, const fs::path& p)
{
	wstring s = L"\"" + p.wstring() + L"\"";
	wstring params;
	error_code e;
	if (!fs::is_directory(p, e))
	{
		s = L"/select," + s; 
		ShellExecuteW(hwnd, NULL, L"c:\\Windows\\explorer.exe", s.c_str(), NULL, SW_SHOWNORMAL);
		return;
	}

	SHELLEXECUTEINFOW sei;
	memset(&sei, 0, sizeof(sei));
	sei.cbSize = sizeof(sei);
	sei.hwnd = hwnd;
	sei.lpVerb = L"explore";
	sei.lpFile = s.c_str();
	sei.lpParameters = params.empty() ? NULL : params.c_str();
	sei.nShow = SW_SHOWNORMAL;
	::ShellExecuteExW(&sei);
}

void ExploreProperties(HWND hwnd, const fs::path& p)
{
	wstring s = L"\"" + p.wstring() + L"\"";
	SHELLEXECUTEINFOW sei;
	memset(&sei, 0, sizeof(sei));
	sei.cbSize = sizeof(sei);
	sei.hwnd = hwnd;
	sei.lpVerb = L"properties";
	sei.lpFile = s.c_str();
	sei.nShow = SW_SHOWNORMAL;
	::ShellExecuteExW(&sei);
}

void CFilesPPDlg::OnNMRClickList2(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

	CMenu contextMenu;
	if (contextMenu.CreatePopupMenu())
	{
		contextMenu.AppendMenu(MF_STRING, MENU_COPYNAMES, _T("Copy Names"));
		contextMenu.AppendMenu(MF_STRING, MENU_COPYPATHS, _T("Copy Full Paths"));
		contextMenu.AppendMenu(MF_STRING, MENU_COPYNAMESQUOTED, _T("Copy Names Quoted"));
		contextMenu.AppendMenu(MF_STRING, MENU_COPYPATHSQUOTED, _T("Copy Full Paths Quoted"));
		contextMenu.AppendMenu(MF_STRING, MENU_EXPLORETO, _T("Explore To"));
		contextMenu.AppendMenu(MF_STRING, MENU_EXPLOREPROPERTIES, _T("Explorer Properties"));

		map<int, std::function<void()>> menuexec;

		if (activeReader)
		{
			int menuid = MENU_SUBSEQUENT;
			//activeReader->AddMenuItems(contextMenu, menuid, menuexec);
		}

		ClientToScreen(&pNMItemActivate->ptAction);
		auto result = contextMenu.TrackPopupMenu(TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_NOANIMATION | TPM_RETURNCMD, pNMItemActivate->ptAction.x, pNMItemActivate->ptAction.y, this);
		
		if (result >= MENU_SUBSEQUENT)
		{
			if (activeReader && menuexec[result]) menuexec[result]();
		}
		else
		{
			CString copyString;
			int lines = 0;

			fs::path activePath;
			if (m_pathCtl.metaPath.GetLocalPath(activePath))
			{
				POSITION pos = m_contentCtl.GetFirstSelectedItemPosition();
				while (pos)
				{
					unsigned nItem = (unsigned)m_contentCtl.GetNextSelectedItem(pos);
					if (nItem < m_contentCtl.filteredItems.size())
					{
						Item& item = *m_contentCtl.filteredItems[nItem];
						fs::path fullpath = activePath / fs::u8path(item.u8Name);

						switch (result)
						{
						case MENU_COPYNAMES: ++lines; copyString += (item.u8Name + "\r\n").c_str();  break;
						case MENU_COPYPATHS: ++lines; copyString += (fullpath.u8string() + "\r\n").c_str(); break;
						case MENU_COPYNAMESQUOTED: ++lines; copyString += ("\"" + item.u8Name + "\"\r\n").c_str();  break;
						case MENU_COPYPATHSQUOTED: ++lines; copyString += ("\"" + fullpath.u8string() + "\"\r\n").c_str(); break;
						case MENU_EXPLORETO: ExploreTo(m_hWnd, fullpath); break;
						case MENU_EXPLOREPROPERTIES: ExploreProperties(m_hWnd, fullpath); break;
						}
					}
				}
			}

			if (result == MENU_COPYNAMES || result == MENU_COPYPATHS || result == MENU_COPYNAMESQUOTED || result == MENU_COPYPATHSQUOTED)
			{
				bool copySucceded = false;
				for (auto start = chrono::steady_clock::now(); chrono::steady_clock::now() - start < 1s; Sleep(1))
				{
					if (OpenClipboard())
					{
						if (EmptyClipboard())
						{
							auto len = copyString.GetLength() + 1;
							if (HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, len))
							{
								auto gptr = GlobalLock(hg);
								memcpy(gptr, (LPCTSTR)copyString, len);
								GlobalUnlock(hg);
								auto b = SetClipboardData(CF_TEXT, hg);
								copySucceded = b == hg;
							}
						}
						CloseClipboard();
						break;
					}
				}

				CString s;
				if (copySucceded) s.Format(_T("Copied %d lines to the clipboard"), (int)lines);
				else s.Format(_T("Copy to clipboard failed"));
				m_statusBar.SetText(s, 255, SBT_NOBORDERS);
			}
		}
	}

	*pResult = 0;
}


class myCOleDataSource : public COleDropSource
{

	virtual SCODE QueryContinueDrag(BOOL bEscapePressed, DWORD dwKeyState) override
	{
		return COleDropSource::QueryContinueDrag(bEscapePressed, dwKeyState);
	}
	virtual SCODE GiveFeedback(DROPEFFECT dropEffect) override
	{
		return COleDropSource::GiveFeedback(dropEffect);
	}
	virtual BOOL OnBeginDrag(CWnd* pWnd) override
	{
		return COleDropSource::OnBeginDrag(pWnd);
	}
};

void CFilesPPDlg::OnLvnBegindragList2(NMHDR *pNMHDR, LRESULT *pResult)
{
	//LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	
	wstring filenamelist;

	fs::path activePath;
	if (m_pathCtl.metaPath.GetLocalPath(activePath))
	{
		POSITION pos = m_contentCtl.GetFirstSelectedItemPosition();
		while (pos)
		{
			unsigned nItem = (unsigned)m_contentCtl.GetNextSelectedItem(pos);
			if (nItem < m_contentCtl.filteredItems.size())
			{
				Item& item = *m_contentCtl.filteredItems[nItem];

				filenamelist += (activePath / fs::u8path(item.u8Name)).wstring();
				filenamelist.append(1, 0);
			}
		}
	}

	if (!filenamelist.empty())
	{
		filenamelist.append(1, 0); // final null to create double terminator
		auto len = sizeof(DROPFILES) + sizeof(wchar_t) * filenamelist.size();
		if (HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, len))
		{
			char* gptr = (char*)GlobalLock(hg);
			memcpy(gptr + sizeof(DROPFILES), filenamelist.data(), sizeof(wchar_t) * filenamelist.size());
			DROPFILES* df = (DROPFILES*)gptr;
			df->pFiles = sizeof(DROPFILES);
			df->pt.x = 0;
			df->pt.y = 0;
			df->fNC = FALSE;
			df->fWide = TRUE;
			GlobalUnlock(hg);

			COleDataSource* ods = new COleDataSource;
			FORMATETC etc = { CF_HDROP, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL };
			ods->CacheGlobalData(CF_HDROP, hg, &etc);

			//ods.CacheGlobalData(CF_TEXT, hMem);
			
			myCOleDataSource dropsource;

			CRect startDragRect;
			GetWindowRect(startDragRect);


			auto resulteffect = ods->DoDragDrop(DROPEFFECT_COPY | DROPEFFECT_MOVE | DROPEFFECT_LINK, &startDragRect, &dropsource);
			resulteffect = 0;
		}
	}
	*pResult = 0;
}


void CFilesPPDlg::OnGetMinMaxInfo(MINMAXINFO* lpMMI)
{
	CDialogEx::OnGetMinMaxInfo(lpMMI);
	lpMMI->ptMinTrackSize.x = std::max<LONG>(lpMMI->ptMinTrackSize.x, 3 * GetSystemMetrics(SM_CXSIZE) + 200);
}


void CFilesPPDlg::OnBnClickedButtonBack()
{
	if (m_pathCtl.NavigateBack())
	{
		LoadContent(true);
	}
}


void CFilesPPDlg::OnBnClickedButtonForward()
{
	if (m_pathCtl.NavigateForward())
	{
		LoadContent(true);
	}
}


void CFilesPPDlg::OnRButtonDown(UINT nFlags, CPoint point)
{
	// TODO: Add your message handler code here and/or call default

	CDialogEx::OnRButtonDown(nFlags, point);
}

Favourites g_favourites;

void CFilesPPDlg::OnDeltaposSpin1(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

	if (pNMUpDown->iDelta > 0)
	{
		if (g_favourites.Toggle(m_pathCtl.metaPath))
		{
			AfxMessageBox(_T("Added favourite"));
		}
		else
		{
			AfxMessageBox(_T("Removed favourite"));
		}
	}
	else if (pNMUpDown->iDelta < 0)
	{
		CMenu m;
		m.CreatePopupMenu();
		auto favourites = g_favourites.copy();
		for (unsigned i = 0; i < favourites.size(); ++i)
		{
			m.AppendMenu(MF_STRING, i + 1, CA2CT(favourites[i].u8DisplayPath().c_str()));
		}
		CRect r;
		m_spinCtrl.GetWindowRect(r);
		int result = m.TrackPopupMenu(TPM_RETURNCMD, r.left, r.bottom, this, r);
		if (result > 0)
		{
			m_pathCtl.metaPath = favourites[result - 1];
			LoadContent(true);
		}
	}
	*pResult = 0;
}


void CFilesPPDlg::OnLvnItemchangedList2(NMHDR *pNMHDR, LRESULT *pResult)
{
	LPNMLISTVIEW pNMLV = reinterpret_cast<LPNMLISTVIEW>(pNMHDR);
	// TODO: Add your control notification handler code here

	if ((pNMLV->uOldState & LVIS_SELECTED) != (pNMLV->uNewState & LVIS_SELECTED) ||
		(pNMLV->uOldState & LVIS_FOCUSED) != (pNMLV->uNewState & LVIS_FOCUSED))
	{

		int foldercount = 0;
		int filecount = 0;
		uint64_t sizesum = 0;

		POSITION pos = m_contentCtl.GetFirstSelectedItemPosition();
		while (pos)
		{
			unsigned nItem = (unsigned)m_contentCtl.GetNextSelectedItem(pos);
			if (nItem < m_contentCtl.filteredItems.size())
			{
				Item& item = *m_contentCtl.filteredItems[nItem];
				if (item.isFolder())
				{
					foldercount += 1;
				}
				else
				{
					filecount += 1;
					sizesum += item.size();
				}
			}
		}

		CString s;
		s.Format(_T("Folders: %llu Files: %llu Size: %llu"), (uint64_t)foldercount, (uint64_t)filecount, (uint64_t)sizesum);
		m_statusBar.SetText(s, 255, SBT_NOBORDERS);
	}

	*pResult = 0;
}