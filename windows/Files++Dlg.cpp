
// FilesPPDlg.cpp : implementation file
//

#include "stdafx.h"
#include "Files++.h"
#include "Files++Dlg.h"
#include "afxdialogex.h"
#include "FilterDlg.h"
#include "FilterDlg.h"
#include "resource.h"
#include "UserPassDialog.h"

#include <chrono>
#include <thread>
#include <algorithm> 
#include <WinIoCtl.h>
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

bool MetaPath::Ascend()
{
	switch (pathType)
	{
	case TopShelf: {
		return false;
	}
	case LocalFS: {
		auto newPath = localPath.parent_path();
		if (newPath == localPath)
		{
			localPath.clear();
			pathType = LocalVolumes;
		}
		else
		{
			localPath = newPath;
		}
		return true;
	}
	case LocalVolumes: {
		pathType = TopShelf;
		return true;
	}
	case MegaAccount: {
		pathType = TopShelf;
		return true;
	}
	case MegaFS: {
		unique_ptr<m::MegaNode> n(masp->getParentNode(mnode.get()));
		if (n)
		{
			mnode.reset(n.release());
		}
		else
		{
			pathType = MegaAccount;
			mnode.reset();
		}
		return true;
	}
	}
	return false;
}

bool MetaPath::Descend(const Item& item)
{
	switch (pathType)
	{
	case TopShelf: {
		if (auto i = dynamic_cast<const ItemMegaAccount*>(&item))
		{
			pathType = MegaAccount;
			masp = i->masp;
			mnode.reset();
		}
		else
		{
			pathType = LocalVolumes;
		}
		return true;
	}
	case LocalFS: {
		fs::path newpath = localPath / fs::u8path(item.u8Name);
		if (fs::is_directory(newpath))
		{
			localPath = newpath;
			return true;
		}
		break;
	}
	case LocalVolumes: {
		auto p = fs::u8path(item.u8Name + "\\");
		if (fs::is_directory(p))
		{
			localPath = p;
			pathType = LocalFS;
			return true;
		}
		break;
	}
	case MegaAccount: {
		if (auto i = dynamic_cast<const ItemMegaNode*>(&item))
		{
			mnode.reset(i->mnode->copy());
			pathType = MegaFS;
			return true;
		}
		else if (auto i = dynamic_cast<const ItemMegaInshare*>(&item))
		{
			mnode.reset(i->mnode->copy());
			pathType = MegaFS;
			return true;
		}
	}
	case MegaFS: {
		if (auto i = dynamic_cast<const ItemMegaNode*>(&item))
		{
			mnode.reset(i->mnode->copy());
			return true;
		}
	}
	}
	return false;
}

void MetaPath::SetLocalPath(const fs::path& p)
{
	pathType = LocalFS;
	localPath = p;
}

bool MetaPath::GetLocalPath(std::filesystem::path& p) const
{
	if (pathType == LocalFS)
	{
		p = localPath;
		return true;
	}
	return false;
}

unique_ptr<FSReader> MetaPath::GetContentReader(HWND wnd, bool recurse) const
{
	switch (pathType)
	{
	case TopShelf: return make_unique<TopShelfReader>(wnd, recurse);
	case LocalFS: return make_unique<LocalFSReader>(localPath, wnd, recurse);
	case LocalVolumes: return make_unique<LocalVolumeReader>(wnd, recurse);
	case MegaAccount: return make_unique<MegaAccountReader>(masp, wnd, recurse);
	case MegaFS: return make_unique<MegaFSReader>(masp, mnode.copy(), wnd, recurse);
	default: assert(false);
	}
	return nullptr;
}

std::string MetaPath::u8DisplayPath() const
{
	switch (pathType)
	{
	case TopShelf: return "<Top Shelf>";
	case LocalFS: return localPath.u8string();
	case LocalVolumes: return "<Local Volumes>";
	case MegaAccount: {
		std::unique_ptr<char[]> e(masp->getMyEmail());
		return string(e.get());
	}
	case MegaFS: {
		std::unique_ptr<char[]> p(masp->getNodePath(mnode.get()));
		return string(p.get());
	}
	}
	return "";
}

bool MetaPath::operator==(const MetaPath& o) const
{
	if (pathType != o.pathType) return false;
	switch (pathType)
	{
	case TopShelf: return true;
	case LocalFS: return localPath == o.localPath;
	case LocalVolumes: return true;
	case MegaAccount: return masp == o.masp;
	case MegaFS: return masp == o.masp && mnode->getHandle() == o.mnode->getHandle();
	}
	return false;
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

struct OneTimeListener : public m::MegaRequestListener
{
	std::function<void(m::MegaRequest *request, m::MegaError* e)> fn;

	OneTimeListener(std::function<void(m::MegaRequest *request, m::MegaError* e)> f) : fn(f) {}

	void onRequestFinish(m::MegaApi*, m::MegaRequest *request, m::MegaError* e) override
	{
		fn(request, e);
		delete this;
	}
};

void FSReader::Send()
{
	if (!currentitems.empty())
	{
		auto v = new vector<unique_ptr<Item>>();
		v->swap(currentitems);
		PostMessage(hwnd, WM_APP_CONTENTUPDATE, (WPARAM)currentaction, (LPARAM)v);
	}
	currentaction = INVALID_ACTION;
}

void FSReader::Queue(Action action, unique_ptr<Item>&& p)
{
	if (currentitems.size() >= 100 || (currentaction != action && !(action == RENAMEDFROM && currentaction == RENAMEDFROM)))
	{
		Send();
	}
	currentaction = action;
	currentitems.emplace_back(move(p));
}

fs::path MEGA::BasePath()
{
	TCHAR szPath[MAX_PATH];
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
	{
		AfxMessageBox(_T("Could not get local appdata path"));
		return fs::path();
	}
	fs::path p(szPath);
	return p / fs::path("Files++");
}


MEGA::MEGA()
{
	fs::path base = BasePath();

	for (auto d = fs::directory_iterator(base); d != fs::directory_iterator(); ++d)
	{
		string sid;
		ifstream f(d->path() / "sid");
		f >> sid;
		if (!sid.empty())
		{
			auto masp = make_shared<m::MegaApi>("BTgWXaYb", d->path().u8string().c_str(), "Files++");
			megaAccounts.push_back(masp);
			masp->fastLogin(sid.c_str(), new OneTimeListener([this, masp](m::MegaRequest *request, m::MegaError* e) { onLogin(*e, masp); }));
		}
	}
}

void MEGA::onLogin(const m::MegaError& e, const shared_ptr<m::MegaApi>& masp)
{
	if (e.getErrorCode()) AfxMessageBox(CA2CT(to_string(e.getErrorCode()).c_str()));
	else
	{
		masp->fetchNodes(new OneTimeListener([](m::MegaRequest *request, m::MegaError* e) 
				{ 
					if (e->getErrorCode()) AfxMessageBox(CA2CT(("fetchnodes: " + to_string(e->getErrorCode())).c_str()));
				}));
	}
}

std::pair<std::shared_ptr<m::MegaApi>, fs::path> MEGA::makeTempAccount()
{
	fs::path p = BasePath() / to_string(time(NULL));
	fs::create_directories(p);
	return make_pair(make_shared<m::MegaApi>("BTgWXaYb", p.u8string().c_str(), "Files++"), p);
}

void MEGA::add(std::shared_ptr<m::MegaApi> p, std::filesystem::path path) {
	std::lock_guard g(m); 
	megaAccounts.push_back(p);
	unique_ptr<char[]> sidvalue(p->dumpSession());
	fs::path sidFile = path / "sid";
	std::ofstream sid(path / "sid");
	sid << sidvalue.get();
}

TopShelfReader::TopShelfReader(HWND targetwindow, bool r)
	: FSReader(targetwindow, r)
	, workerthread([this]() { Threaded(); })
{
}

TopShelfReader::~TopShelfReader()
{
	cancelling = true;
	workerthread.join();
}

void TopShelfReader::Threaded()
{
	Queue(NEWITEM, make_unique<Item>("<Local Volumes>"));
	auto megaAccounts = g_mega->accounts();
	for (auto& p : megaAccounts)
	{
		unique_ptr<char[]> email(p->getMyEmail());
		Queue(NEWITEM, make_unique<ItemMegaAccount>(email.get() ? email.get() : "<loading>", p));
	}
	Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
	Send();
}

void TopShelfReader::AddMEGAAccount()
{
	auto [masp, path] = g_mega->makeTempAccount();

	UserPassDialog dlg;
	for (;;)
	{
		if (IDOK == dlg.DoModal())
		{
			bool done = false, success = false;
			masp->login(CT2CA(dlg.name), CT2CA(dlg.password), new OneTimeListener([&](m::MegaRequest *, m::MegaError* e) { success = e->getErrorCode() == m::MegaError::API_OK; done = true;  }));
			while (!done) Sleep(100);
			if (success)
			{
				g_mega->add(masp, path);
				AfxMessageBox(_T("Login succeeded"));
				g_mega->onLogin(m::MegaError(0), masp);
				return;
			}
			AfxMessageBox(_T("Login failed"));
			continue;
		}
		else break;
	}
	std::error_code ec;
	fs::remove_all(path, ec);
}

void TopShelfReader::AddMenuItems(CMenu& m, int& id, map<int, std::function<void()>>& me)
{
	m.AppendMenu(MF_SEPARATOR, id++, LPCTSTR(nullptr));

	m.AppendMenu(MF_STRING, id, _T("Add MEGA Account"));
	me[id] = [this]() { AddMEGAAccount();  };
	++id;
}

LocalVolumeReader::LocalVolumeReader(HWND targetwindow, bool r)
	: FSReader(targetwindow, r)
	, workerthread([this]() { Threaded(); })
{
	memset(&overlapped, 0, sizeof(overlapped));
}

LocalVolumeReader::~LocalVolumeReader()
{
	cancelling = true;
	workerthread.join();
}

void LocalVolumeReader::Threaded()
{
	wstring s;
	DWORD n = GetLogicalDriveStringsW(0, s.data());
	s.resize(n);
	n = GetLogicalDriveStringsW(n, s.data());

	while (s.length() > 1)
	{
		auto len2 = wcslen(s.c_str());
		auto len = len2;
		if (len > 0 && s.c_str()[len - 1] == _T('\\')) --len;
		if (len > 0)
		{
			string n = wstring_convert<codecvt_utf8_utf16<wchar_t>, wchar_t>{}.to_bytes(s.substr(0, len));

			ULARGE_INTEGER freebytesforcaller, totalbytes, freebytes;
			GetDiskFreeSpaceExA(n.c_str(), &freebytesforcaller, &totalbytes, &freebytes);

			Queue(NEWITEM, make_unique<ItemLocalFS>(n, totalbytes.QuadPart, true));
		}
		s.erase(0, len2 + 1);
	}
	Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
	Send();
}

LocalFSReader::LocalFSReader(fs::path p, HWND targetwindow, bool r) 
	: FSReader(targetwindow, r)
	, dir(p)
{
	memset(&overlapped, 0, sizeof(overlapped));
	workerthread = std::thread([this]() { Threaded(); });
}

LocalFSReader::~LocalFSReader()
{
	cancelling = true;
	if (hDirectory != INVALID_HANDLE_VALUE) CancelIoEx(hDirectory, &overlapped);
	workerthread.join();
}

bool LocalFSReader::ReadDir(const fs::path& p, bool recurse, const fs::path& recurseprefix)
{
	// read directory now
	try
	{
		int insertPos = 0;
		for (fs::directory_iterator i(p); i != fs::directory_iterator(); ++i, ++insertPos)
		{
			fs::path relativename = recurseprefix / i->path().filename();
			if (i->is_directory())
			{
				Queue(NEWITEM, make_unique<ItemLocalFS>(relativename.u8string(), -1, true));
				if (recurse && !ReadDir(i->path(), true, relativename)) return false;
			}
			else if (i->is_regular_file())
			{
				Queue(NEWITEM, make_unique<ItemLocalFS>(relativename.u8string(), i->file_size(), false));
			}
		}
		return true;
	}
	catch (std::exception& )
	{
		//Queue(FILE_ACTION_APP_ERROR, unique_ptr<Item>(new ItemError("Error reading directory: " + string(e.what()))));
		//AfxMessageBox(e.what(), IDOK);
		//return false;
		return true;
	}
}

bool LocalFSReader::RequestChanges()
{
	return ReadDirectoryChangesW(hDirectory, (LPVOID)overlappedbuf.data(), (DWORD)overlappedbuf.size(), recurse,
		FILE_NOTIFY_CHANGE_FILE_NAME
		| FILE_NOTIFY_CHANGE_DIR_NAME
		| FILE_NOTIFY_CHANGE_LAST_WRITE
		| FILE_NOTIFY_CHANGE_SIZE
		| FILE_NOTIFY_CHANGE_CREATION, NULL, &overlapped, NULL);
}


std::wstring checkIfFolderIsReparsePoint(HANDLE h)
{
	// from ms doco: not available in headers
	typedef struct _REPARSE_DATA_BUFFER {
		ULONG  ReparseTag;
		USHORT ReparseDataLength;
		USHORT Reserved;
		union {
			struct {
				USHORT SubstituteNameOffset;
				USHORT SubstituteNameLength;
				USHORT PrintNameOffset;
				USHORT PrintNameLength;
				ULONG  Flags;
				WCHAR  PathBuffer[1];
			} SymbolicLinkReparseBuffer;
			struct {
				USHORT SubstituteNameOffset;
				USHORT SubstituteNameLength;
				USHORT PrintNameOffset;
				USHORT PrintNameLength;
				WCHAR  PathBuffer[1];
			} MountPointReparseBuffer;
			struct {
				UCHAR DataBuffer[1];
			} GenericReparseBuffer;
		} DUMMYUNIONNAME;
	} REPARSE_DATA_BUFFER, *PREPARSE_DATA_BUFFER;

	// Allocate the reparse data structure
	DWORD dwBufSize = MAXIMUM_REPARSE_DATA_BUFFER_SIZE;
	unique_ptr<char[]> rdatabuf(new char[dwBufSize]);
	PREPARSE_DATA_BUFFER rdata = (PREPARSE_DATA_BUFFER)rdatabuf.get();

	// Query the reparse data
	DWORD dwRetLen;
	if (rdata && 
		DeviceIoControl(h, FSCTL_GET_REPARSE_POINT, NULL, 0, rdata, dwBufSize, &dwRetLen, NULL) &&
		IsReparseTagMicrosoft(rdata->ReparseTag))
	{
		if (rdata->ReparseTag == IO_REPARSE_TAG_SYMLINK)
		{
			size_t slen = rdata->SymbolicLinkReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
			if (slen > 0) return std::wstring(&rdata->SymbolicLinkReparseBuffer.PathBuffer[rdata->SymbolicLinkReparseBuffer.SubstituteNameOffset / sizeof(WCHAR)], slen);
			size_t plen = rdata->SymbolicLinkReparseBuffer.PrintNameLength / sizeof(WCHAR);
			if (plen > 0) return std::wstring(&rdata->SymbolicLinkReparseBuffer.PathBuffer[rdata->SymbolicLinkReparseBuffer.PrintNameOffset / sizeof(WCHAR)], plen);
		}
		else if (rdata->ReparseTag == IO_REPARSE_TAG_MOUNT_POINT)
		{
			size_t slen = rdata->MountPointReparseBuffer.SubstituteNameLength / sizeof(WCHAR);
			if (slen > 0) return std::wstring(&rdata->MountPointReparseBuffer.PathBuffer[rdata->MountPointReparseBuffer.SubstituteNameOffset / sizeof(WCHAR)], slen);
			size_t plen = rdata->MountPointReparseBuffer.PrintNameLength / sizeof(WCHAR);
			if (plen > 0) return std::wstring(&rdata->MountPointReparseBuffer.PathBuffer[rdata->MountPointReparseBuffer.PrintNameOffset / sizeof(WCHAR)], plen);
		}
	}
	return std::wstring();
}

void LocalFSReader::Threaded()
{
	hDirectory = CreateFileW(dir.wstring().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE , NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
	if (hDirectory == INVALID_HANDLE_VALUE)
	{
		error_code ec((int)GetLastError(), system_category());

		// check if we can open it as a reparse point (soft link) instead
		hDirectory = CreateFileW(dir.wstring().c_str(), FILE_READ_EA, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
		if (hDirectory != INVALID_HANDLE_VALUE)
		{
			// get the reparse point actual path

			std::wstring actualfolder = checkIfFolderIsReparsePoint(hDirectory);
			if (!actualfolder.empty())
			{
				hDirectory = CreateFileW(actualfolder.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
				if (hDirectory != INVALID_HANDLE_VALUE)
				{
					if (actualfolder.find(L"\\\\?\\") == 0) actualfolder.erase(0, 4);  // extended-length path prefix "\\?\"
					if (actualfolder.find(L"\\??\\") == 0) actualfolder.erase(0, 4);  // seen in practice "\??\"
					dir = fs::path(actualfolder);
					Queue(FOLDER_RESOLVED_SOFTLINK, make_unique<ItemLocalFS>(dir.u8string(), -1, true));
				}
			}
		}

		if (hDirectory == INVALID_HANDLE_VALUE)
		{
			Queue(FILE_ACTION_APP_ERROR, unique_ptr<Item>(new ItemError("Could not read directory: " + ec.message())));
			Send();
			return;
		}
	}

	// overlapped so we can make a first call now, which initialises its internal buffer, so we don't lose any notifications that occur during initial scan (unless overflow occurs)
	memset(&overlapped, 0, sizeof(overlapped));
	overlappedbuf.resize(48 * 1024);
	bool notifyOk = RequestChanges();

	ReadDir(dir, recurse, "");
	Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
	Send();

	// and scan for changes (including any that occured while reading)
	DWORD dwBytes = 0;
	while (notifyOk && !cancelling && GetOverlappedResultEx(hDirectory, &overlapped, &dwBytes, INFINITE, FALSE))
	{
		char* ptr = (char*)overlappedbuf.data();
		while ((int)dwBytes >= sizeof(FILE_NOTIFY_INFORMATION))
		{
			FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)ptr;

			if ((char*)&fni->FileName + fni->FileNameLength - (char*)fni > (int)dwBytes) break;

			fs::path p(wstring(fni->FileName, fni->FileNameLength/sizeof(wchar_t)));

			switch (fni->Action)
			{
			case FILE_ACTION_ADDED:		
				{
					error_code ec;
					auto s = fs::directory_entry(dir / p, ec);
					if (!ec && fs::is_directory(s)) Queue(NEWITEM, make_unique<ItemLocalFS>(p.u8string(), -1, true));
					if (!ec && fs::is_regular_file(s)) Queue(NEWITEM, make_unique<ItemLocalFS>(p.u8string(), s.file_size(), false));
					break;
				}
			case FILE_ACTION_REMOVED:	
				Queue(DELETEDITEM, make_unique<ItemLocalFS>(p.u8string(), -1, false));
				break;

			case FILE_ACTION_RENAMED_OLD_NAME:
				Queue(RENAMEDFROM, make_unique<ItemLocalFS>(p.u8string(), -1, false));
				break;

			case FILE_ACTION_RENAMED_NEW_NAME:
				Queue(RENAMEDTO, make_unique<ItemLocalFS>(p.u8string(), -1, false));
				break;
			}

			ptr += fni->NextEntryOffset;
			dwBytes -= fni->NextEntryOffset ? fni->NextEntryOffset : dwBytes;
		}
		Send();

		// kick off a new query (notifications in the meantime will have been queued in its internal buffer)
		notifyOk = RequestChanges();
	}

	if (!cancelling && !notifyOk)
	{
		Queue(FILE_ACTION_APP_NOTIFY_FAILURE, NULL);
		Send();
	}
}

MegaAccountReader::MegaAccountReader(std::shared_ptr<m::MegaApi> p, HWND targetwindow, bool r)
	: FSReader(targetwindow, r)
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

MegaFSReader::MegaFSReader(std::shared_ptr<m::MegaApi> p, std::unique_ptr<m::MegaNode> n, HWND targetwindow, bool r)
	: FSReader(targetwindow, r)
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

	activeReader = m_pathCtl.metaPath.GetContentReader(m_hWnd, filterSettings.recursesubfolders);
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
	while (ra != a.end())
	{
		if (writinga) *(wa++) = move(*(ra++));
	}
	a.erase(wa, a.end());
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
	unique_ptr<vector<unique_ptr<Item>>> newitems((vector<unique_ptr<Item>>*)lParam);

	switch (FSReader::Action(wParam))
	{
	case FSReader::NEWITEM:
	case FSReader::RENAMEDTO:
		if (filter) filter->FilterNewItems(*newitems);

		//items.reserve(items.size() + newitems->size());
		for (auto& i : *newitems) items.emplace_back(move(i));
		break;

	case FSReader::DELETEDITEM:
	case FSReader::RENAMEDFROM:
		sort_match_removeintersection(items, *newitems, m_pathCtl.metaPath.nodeCompare());
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
			activeReader->AddMenuItems(contextMenu, menuid, menuexec);
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
