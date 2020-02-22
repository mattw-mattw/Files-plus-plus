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
#include <fstream>
#include <sstream>
#include "../core/PlatformSupplied.h"
#include "../core/MEGA.h"

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


IMPLEMENT_DYNAMIC(ItemListCtrl, CMFCListCtrl);
BEGIN_MESSAGE_MAP(ItemListCtrl, CMFCListCtrl)
    ON_WM_DROPFILES()
    ON_NOTIFY_EX(TTN_NEEDTEXT, 0, &ItemListCtrl::OnTTNNeedText)
END_MESSAGE_MAP()

void ItemListCtrl::OnDropFiles(HDROP hDropInfo)
{
    dlg.OnDropFiles(hDropInfo);
}

void ItemListCtrl::Sort(int iColumn, BOOL bAscending, BOOL bAdd)
{
    Sort(iColumn, bAscending, bAdd, false);
}

void ItemListCtrl::Sort(int iColumn, BOOL bAscending, BOOL bAdd, bool initial)
{
    if (iColumn < dlg.activeReader->columnDefs.size())
    {
        std::function<bool(const Item* a, const Item* b)> sortFunction;
        if (initial)
        {
            auto f = dlg.activeReader->columnDefs[iColumn].initialSort;
            if (!f) return;
            sortFunction = [f](const Item* a, const Item* b) { return f(a) < f(b); };
        }
        else if (dlg.activeReader->columnDefs[iColumn].intFunc)
        {
            auto f = dlg.activeReader->columnDefs[iColumn].intFunc;
            if (bAscending) sortFunction = [f](const Item* a, const Item* b) { return f(a) < f(b); };
            else            sortFunction = [f](const Item* a, const Item* b) { return f(b) < f(a); };
        }
        else
        {
            auto f = dlg.activeReader->columnDefs[iColumn].valueFunc;
            if (bAscending) sortFunction = [f](const Item* a, const Item* b) { return f(a) < f(b); };
            else            sortFunction = [f](const Item* a, const Item* b) { return f(b) < f(a); };
        }

        dlg.filteredItems.sort(sortFunction);

        GetHeaderCtrl().SetSortColumn(iColumn, bAscending, bAdd);

        m_iSortedColumn = iColumn;
        m_bAscending = bAscending;

        InvalidateRect(NULL);
    }
}



// CFilesPPDlg dialog


IMPLEMENT_DYNAMIC(CFilesPPDlg, CDialogEx);

CFilesPPDlg::CFilesPPDlg(CWnd* pParent /*=nullptr*/)
    : CDialogEx(IDD_FILESPP_DIALOG, pParent)
    , m_contentCtl(*this)
    , items([](const unique_ptr<Item>& a, const unique_ptr<Item>& b) { return a->u8Name < b->u8Name; })
    , filteredItems([](const Item* a, const Item* b) { return a->u8Name < b->u8Name; })
{
    if (!g_mega)
    {
        g_mega = new MEGA;
        g_mega->loadFavourites(nullptr);
    }

    EnableActiveAccessibility();
    m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    ++atomicInstances;
}

void CFilesPPDlg::OnDestroy()
{
    CDialogEx::OnDestroy();
}

CFilesPPDlg::~CFilesPPDlg()
{
    activeReader.reset();

    if (!--atomicInstances)
    {
        delete g_mega;
        ++atomicExitOk;
        shutdown_cv.notify_all();
    }
}

std::atomic<unsigned> CFilesPPDlg::atomicInstances{ 0 };
std::atomic<unsigned> CFilesPPDlg::atomicExitOk{ 0 };

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
    ON_WM_DESTROY()
    ON_WM_PAINT()
    ON_WM_QUERYDRAGICON()
    ON_BN_CLICKED(IDC_BUTTON1, &CFilesPPDlg::OnBnClickedButton1)
    ON_NOTIFY(NM_DBLCLK, IDC_LIST2, &CFilesPPDlg::OnNMDblclkList2)
    ON_BN_CLICKED(IDC_BUTTON2, &CFilesPPDlg::OnBnClickedButton2)
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
    ON_NOTIFY(LVN_GETDISPINFO, IDC_LIST2, &CFilesPPDlg::OnLvnGetdispinfoList2)
END_MESSAGE_MAP()


IMPLEMENT_DYNAMIC(PathCtrl, CEdit);
BEGIN_MESSAGE_MAP(PathCtrl, CEdit)
    ON_WM_DROPFILES()
END_MESSAGE_MAP()

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
    metaPath.reset(new MetaPath_LocalFS(StartPath()));
}

void PathCtrl::SetWindowText()
{
    ::SetWindowTextA(m_hWnd, metaPath->u8DisplayPath().c_str());
}

void PathCtrl::AcceptEdits()
{
    CString csPath;
    GetWindowText(csPath);
    metaPath.reset(new MetaPath_LocalFS(fs::u8path(string(CT2CA(csPath)))));  // todo: recognise other path types
}

void PathCtrl::PrepareNavigateBack()
{
    if (!historyBack.empty() && *historyBack.back() == *metaPath) return;
    historyBack.push_back(metaPath->clone());
    historyForward.clear();
}

bool PathCtrl::NavigateBack()
{
    if (historyBack.empty()) return false;
    historyForward.push_back(metaPath->clone());
    metaPath = move(historyBack.back());
    historyBack.pop_back();
    return true;
}

bool PathCtrl::NavigateForward()
{
    if (historyForward.empty()) return false;
    historyBack.push_back(metaPath->clone());
    metaPath = move(historyForward.back());
    historyForward.pop_back();
    return true;
}

CRect GetLocalRectIn(CWnd*w, CWnd* pw) {
    CRect r;
    w->GetWindowRect(r);
    pw->ScreenToClient((POINT*)&r);
    pw->ScreenToClient(((POINT*)&r)+1);
    return r;
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
            pSysMenu->AppendMenu(MF_STRING, IDM_FONTBOX, L"Choose Font");
            pSysMenu->AppendMenu(MF_STRING, IDM_COMPARE_TO_FAVE, L"Compare To Favourite");
            pSysMenu->AppendMenu(MF_STRING, IDM_COMPARE_TO_FAVE_DIFFONLY, L"Compare To Favourite (Different only)");
        }
    }

    // Set the icon for this dialog.  The framework does this automatically
    //  when the application's main window is not a dialog
    SetIcon(m_hIcon, TRUE);			// Set big icon
    SetIcon(m_hIcon, FALSE);		// Set small icon

    m_contentCtl.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES | m_contentCtl.GetExtendedStyle()); 
    //m_contentCtl.SendMessage(LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
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

    CRect localrect;
    GetClientRect(localrect);
    CRect cwr = GetLocalRectIn(&m_contentCtl, this);
    CRect sbr = GetLocalRectIn(&m_statusBar, this);
    CRect fbr = GetLocalRectIn(&m_filterButton, this);
    CRect scr = GetLocalRectIn(&m_spinCtrl, this);
    CRect pcr = GetLocalRectIn(&m_pathCtl, this);
    
    m_contentCtl.MoveWindow(0, fbr.bottom, localrect.Width(), localrect.Height()-fbr.bottom-sbr.Height());
    m_filterButton.MoveWindow(localrect.Width()-fbr.Width(), 0, fbr.Width(), fbr.Height());
    m_spinCtrl.MoveWindow(localrect.Width()-fbr.Width()-scr.Width(), 0, scr.Width(), fbr.Height());
    m_pathCtl.MoveWindow(pcr.left, 0, localrect.Width()-fbr.Width()-scr.Width()-pcr.left, fbr.Height());

    GetDynamicLayout()->AddItem(m_contentCtl.m_hWnd, CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontalAndVertical(100,100));
    GetDynamicLayout()->AddItem(m_filterButton.m_hWnd, CMFCDynamicLayout::MoveHorizontal(100), CMFCDynamicLayout::SizeNone());
    GetDynamicLayout()->AddItem(m_spinCtrl.m_hWnd, CMFCDynamicLayout::MoveHorizontal(100), CMFCDynamicLayout::SizeNone());
    GetDynamicLayout()->AddItem(m_pathCtl.m_hWnd, CMFCDynamicLayout::MoveNone(), CMFCDynamicLayout::SizeHorizontal(100));


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

void CFilesPPDlg::LoadContent(bool resetFilter)
{
    string title = "Files++";
    if (auto acc = m_pathCtl.metaPath->Account())
    {
        OwnString email(acc->masp->getMyEmail());
        if (!email.empty()) title = email;
    }
    SetWindowText(CA2CT(title.c_str()));
    
    m_pathCtl.SetWindowText();
    m_contentCtl.SetItemCount(0);
    items.clear();
    filteredItems.clear();

    if (resetFilter)
    {
        if (filterSettings.resetonnavigate) filterSettings.Reset();
        filterSettings.recursesubfolders = false;
    }
    filter.reset(new Filter(filterSettings, *this));

    QueueTrigger t = [wnd = m_hWnd]() { ::PostMessage(wnd, WM_APP_CONTENTUPDATE, (WPARAM)0, (LPARAM)0); };

    activeReader = m_pathCtl.metaPath->GetContentReader(t, filterSettings.recursesubfolders, *this);

    while (m_contentCtl.GetHeaderCtrl().GetItemCount() > 0) m_contentCtl.DeleteColumn(0);

    int col = 0;
    for (auto& cd : activeReader->columnDefs)
    {
        m_contentCtl.InsertColumn(col++, CA2CT(cd.name.c_str()), cd.intFunc ? LVCFMT_RIGHT : LVCFMT_LEFT, cd.width, 1);
    }
    m_contentCtl.InsertColumn(col, _T(""), LVCFMT_RIGHT, 10, 2);  // nothing shown for this one - makes autosizing work better    
}


//template<class T1, class T2>
//void remove_if_absent(T1& a, T2& b, std::function<bool(std::unique_ptr<T>&, typename T2::value_type&)> cmpless)
//{
//    T1::iterator ra = a.begin(), wa = a.begin();
//    bool writinga = false;
//    while (ra != a.end())
//    {
//        unique_ptr<Item> hackptr(*ra);
//        if (binary_search(b.begin(), b.end(), hackptr))
//        {
//            if (writinga) *wa = move(*ra);
//            ++wa, ++ra;
//        }
//        else
//        {
//            ++ra;
//            writinga = true;
//        }
//        hackptr.release();
//    }
//    a.erase(wa, a.end());
//}

LRESULT CFilesPPDlg::OnContentUpdate(WPARAM wParam, LPARAM lParam)
{
    
    ItemQueue::Entry entry;
    while (activeReader->itemQueue->q.pop(entry, false))
    {
        switch (entry.action)
        {
        case NEWITEM:
        case RENAMEDTO:
            if (filter) filter->FilterNewItems(entry.batch);

            for (auto& i : entry.batch) items.add(move(i));
            break;

        case DELETEDITEM:
        case RENAMEDFROM:
            {
                std::deque<Item*> batch;
                //batch.reserve(entry.batch.size());
                for (auto& i : entry.batch) batch.push_back(i.get());
                filteredItems.removeintersection(batch, true);
            }
            items.removeintersection(entry.batch, false);
            //remove_if_absent(m_contentCtl.filteredItems, entry.batch);
            m_contentCtl.SetItemCount(int(filteredItems.size()));
            break;

        case FILE_ACTION_APP_ERROR:
            AfxMessageBox(_T("The directory could not be read"));
            break;

        case FILE_ACTION_APP_NOTIFY_FAILURE:
            AfxMessageBox(_T("Notifications are not working, file changes will not be reflected"));
            break;

        case FILE_ACTION_APP_READCOMPLETE:
            ContentEstablished();
            break;

        case FILE_ACTION_APP_READRESTARTED:
            m_contentCtl.SetItemCount(0);
            filteredItems.clear();;
            items.clear();
            break;

        case FOLDER_RESOLVED_SOFTLINK:
            if (auto p = dynamic_cast<LocalFSReader*>(activeReader.get()))
            {
                m_pathCtl.metaPath.reset(new MetaPath_LocalFS(p->dir));
                m_pathCtl.SetWindowText();
            }
            break;
        }

    }
    
    return 0;
}


void CFilesPPDlg::ContentEstablished()
{
    for (size_t col = activeReader->columnDefs.size(); col--; )
    {
        if (activeReader->columnDefs[col].initialSort)
        {
            m_contentCtl.Sort(int(col), TRUE, FALSE, true);
            break;
        }
    }

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
    if ((nID & 0xFFF0) == IDM_FONTBOX)
    {
        CFontDialog dlg;
        if (dlg.DoModal() == IDOK)
        {
            // Create the font using the selected font from CFontDialog.
            LOGFONT lf;
            memcpy(&lf, dlg.m_cf.lpLogFont, sizeof(LOGFONT));

            auto font = new CFont;
            VERIFY(font->CreateFontIndirect(&lf));

            m_contentCtl.SetFont(font, TRUE);
        }
    }
    if ((nID & 0xFFF0) == IDM_COMPARE_TO_FAVE || (nID & 0xFFF0) == IDM_COMPARE_TO_FAVE_DIFFONLY)
    {
        bool differentOnly = (nID & 0xFFF0) == IDM_COMPARE_TO_FAVE_DIFFONLY;
        CMenu m;
        m.CreatePopupMenu();
        MenuActions ma;
        ma.actions.push_back(MenuActions::MenuItem{ string("*** Compare To ***") , nullptr, true });
        auto favourites = g_mega->favourites.copy();
        OwningAccountPtr currAcc = nullptr;
        for (unsigned i = 0; i < favourites.size(); ++i)
        {
            auto test = favourites[i]->Account();
            if (test != currAcc)
            {
                currAcc = test;
                ma.actions.push_back(MenuActions::MenuItem{ currAcc ? OwnString(currAcc->masp->getMyEmail()) : string("<Local>") , nullptr, true });
            }
            ma.actions.push_back(MenuActions::MenuItem{ favourites[i]->u8DisplayPath(), [thisfavourite = std::shared_ptr<MetaPath>(favourites[i]->clone()), differentOnly, this]()
            {
                // make a new top level window (complete with its own gui thread) to load subdir 
                auto t = make_unique<CFilesPPDlgThread>();
                auto view1 = m_pathCtl.metaPath->clone();
                auto view2 = thisfavourite->clone();
                auto dc1 = dynamic_cast<MetaPath_LocalFS*>(view1.get());
                auto dc2 = dynamic_cast<MetaPath_LocalFS*>(view2.get());
                if (dc1 && !dc2) dc1->normalizeSeparator = true;
                if (dc2 && !dc1) dc2->normalizeSeparator = true;
                t->dlg.m_pathCtl.metaPath = std::make_unique<MetaPath_CompareView>(move(view1), move(view2), differentOnly);
                t->m_bAutoDelete = true;
                GetWindowRect(t->dlg.originatorWindowRect);
                if (!t->CreateThread()) AfxMessageBox(_T("Thread creation failed"));
                else t.release();
            } });
        }
        CRect r;
        m_spinCtrl.GetWindowRect(r);
        ExecMenu(m, ma, 100, POINT{ r.left, r.bottom }, this);
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
    if (auto tmp = m_pathCtl.metaPath->Ascend())
    {
        m_pathCtl.PrepareNavigateBack();
        m_pathCtl.metaPath = move(tmp);
        LoadContent(true);
    }
}

void CFilesPPDlg::OnNMDblclkList2(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

    if (pNMItemActivate->iItem >= 0 && pNMItemActivate->iItem < m_contentCtl.GetItemCount() && (unsigned)pNMItemActivate->iItem < filteredItems.size())
    {
        Item& item = *filteredItems[pNMItemActivate->iItem];

        //CString s = m_contentCtl.GetItemText(pNMItemActivate->iItem, 0);

        //// remove trailing \ for directories
        //if (s.GetLength() > 0 && s[s.GetLength() - 1] == _T('\\'))
        //{
        //	s.Delete(s.GetLength() - 1, 1);
        //}

        if (pNMItemActivate->uKeyFlags & LVKF_SHIFT)
        {
            // make a new top level window (complete with its own gui thread) to load subdir 
            if (auto tmp = m_pathCtl.metaPath->Descend(item))
            {
                auto t = make_unique<CFilesPPDlgThread>();
                t->dlg.m_pathCtl.metaPath = move(tmp);
                t->m_bAutoDelete = true;
                t->dlg.filterSettings = filterSettings;
                GetWindowRect(t->dlg.originatorWindowRect);
                if (!t->CreateThread()) AfxMessageBox(_T("Thread creation failed"));
                else t.release();
            }
        }
        else
        {
            if (auto tmp = m_pathCtl.metaPath->Descend(item))
            {
                // load subdir in this window
                m_pathCtl.PrepareNavigateBack();
                m_pathCtl.metaPath = move(tmp);
                LoadContent(true);
            }
        }
    }
    *pResult = 0;
}


void CFilesPPDlg::OnBnClickedButton2()
{
    //if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
    {
        modelessFilter.reset(new FilterDlg);
        modelessFilter->m_settings = modelessFilter->m_settings = filterSettings;

        modelessFilter->onOkAction = [this]() {
            filterSettings = modelessFilter->m_settings;
            LoadContent(false);
        };
        modelessFilter->Create(IDD_FILTERDIALOG, this);
        modelessFilter->ShowWindow(SW_SHOW);
        return;
    }
    //else
    //{
    //    FilterDlg dlg;
    //    dlg.m_settings = dlg.m_settings = filterSettings;
    //    switch (dlg.DoModal())
    //    {
    //    case IDOK:
    //        if (filterSettings.recursesubfolders != dlg.m_settings.recursesubfolders)
    //        {
    //            filterSettings = dlg.m_settings;
    //            LoadContent(false);
    //        }
    //        else
    //        {
    //            filterSettings = dlg.m_settings;
    //            filter.reset(new Filter(filterSettings, *this));
    //            filter->FilterNewItems(items);
    //        }
    //        break;
    //    case IDCANCEL:
    //        break;
    //    }
    //}
}

Filter::Filter(const FilterDlg::Settings& filterSettings, UserFeedback& uf) : userFeedback(uf)
{
    string exp = CT2CA(filterSettings.text);
    if (!filterSettings.regularexpression)
    {
        // convert non-re to re
        for (size_t i = exp.size(); i--; )
        {
            if (exp[i] == '?') exp[i] = '.';
            else if (exp[i] == '*') exp.replace(i, 1, ".*");
            else exp.insert(i, "\\");
        }
    }

    try
    {
        re = std::regex(exp.begin(), exp.end(), filterSettings.casesensitive ? std::regex::ECMAScript : std::regex::icase);
    }
    catch (std::exception & e)
    {
        ReportError("Regular Expression error: " + std::string(e.what()));
        return;
    }

    noFolders = filterSettings.filtertype == "Hide folders";
    noFiles = filterSettings.filtertype == "Hide files";
    filterFolders = filterSettings.filtertype == "Filter all" || noFiles;

    string buttonText;
    if (noFolders) buttonText = "Files: ";
    if (noFiles) buttonText = "Folders: ";
    buttonText += CT2CA(filterSettings.text.IsEmpty() ? CString(".*") : filterSettings.text);
    uf.SetFilterText(buttonText);
    uf.ClearFilteredItems();
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
        userFeedback.AddFilteredItem(p.get());
        if (p->isFolder()) ++folders;
        else ++files, size += p->size();
    }
    userFeedback.DoneAddingFilteredItems();
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
        userFeedback.AddFilteredItem(p.get());
        if (p->isFolder()) ++folders;
        else ++files, size += p->size();
    }
    userFeedback.DoneAddingFilteredItems();
}

void Filter::ApplyStatus()
{
    userFeedback.SetUserFeedback(ostringstream() << "Folders: " << folders << " Files: " << files << "  Size: " << size);
}

BOOL ItemListCtrl::OnTTNNeedText( UINT id, NMHDR * pNMHDR, LRESULT * pResult )
{
    TOOLTIPTEXTW* pTTTW = (TOOLTIPTEXTW*)pNMHDR;
    *pResult = 0;

    LVHITTESTINFO lvhitTestInfo;
    lvhitTestInfo.pt = GetCurrentMessage()->pt;
    ScreenToClient( &lvhitTestInfo.pt );

    int nItem = SubItemHitTest(&lvhitTestInfo);
    int nSubItem = lvhitTestInfo.iSubItem;

    if (lvhitTestInfo.flags & LVHT_ONITEM)
    {
        if (nItem < dlg.filteredItems.size() && 
            nSubItem < dlg.activeReader->columnDefs.size()) 
        {
            if (auto& f = dlg.activeReader->columnDefs[nSubItem].valueFunc)
            {
                static std::wstring tooltipText;
                //wcsncpy_s(pItem->pszText, pItem->cchTextMax, str, min(str.GetLength(), pItem->cchTextMax-1));
                tooltipText.assign(CA2CT(f(dlg.filteredItems[nItem]).c_str()));
                for (auto i = tooltipText.size(); i--; ) if (tooltipText[i] == '\n') tooltipText.replace(i, 1, L"\r\n");
                ::SendMessage(pTTTW->hdr.hwndFrom, TTM_SETMAXTIPWIDTH, 0, 1000);
                pTTTW->lpszText = (LPWSTR)tooltipText.c_str();
            }
        }
    }
    return FALSE;
}

void CFilesPPDlg::OnLvnGetdispinfoList2(NMHDR* pNMHDR, LRESULT* pResult)
{
    NMLVDISPINFO* pDispInfo = reinterpret_cast<NMLVDISPINFO*>(pNMHDR);
    LV_ITEM* pItem = &(pDispInfo)->item;

    unsigned iItem = (unsigned)pItem->iItem;

    if (iItem < filteredItems.size() && (pItem->mask & LVIF_TEXT) && //valid text buffer?
        pItem->iSubItem < activeReader->columnDefs.size()) 
    {
        if (auto& f = activeReader->columnDefs[pItem->iSubItem].valueFunc)
        {
            CString str(CA2WEX(f(filteredItems[iItem]).c_str(), CP_UTF8));
            wcsncpy_s(pItem->pszText, pItem->cchTextMax, str, min(str.GetLength(), pItem->cchTextMax-1));
        }
    }
    *pResult = 0;
}

INT_PTR ItemListCtrl::OnToolHitTest(CPoint point, TOOLINFO* pTI) const 
{
   // CPoint pt(GetMessagePos());
    //ScreenToClient(&point);

    LVHITTESTINFO lvhitTestInfo;
    lvhitTestInfo.pt = point;
    int nItem = const_cast<ItemListCtrl*>(this)->SubItemHitTest(&lvhitTestInfo);
    int nSubItem = lvhitTestInfo.iSubItem;

    if (!(lvhitTestInfo.flags & LVHT_ONITEM)) return -1;

    //Fill in the TOOLINFO structure
    pTI->uFlags |= TTF_ALWAYSTIP;
    pTI->hwnd = m_hWnd;
    pTI->uId = (UINT) (nItem * 100 + nSubItem);
    pTI->lpszText = LPSTR_TEXTCALLBACK;
    GetClientRect(&pTI->rect);
//    pTI->rect = rcClient;

//    RECT rcClient;
//    GetClientRect( &rcClient );

    ////Fill in the TOOLINFO structure
    //pTI->hwnd = m_hWnd;
    //pTI->uId = (UINT) (nRow * 1000 + nCol);
    //// Send TTN_NEEDTEXT when tooltip should be shown
    //pTI->lpszText = LPSTR_TEXTCALLBACK;
    //pTI->rect = rcClient;

    return pTI->uId;
}


void CFilesPPDlg::OnDropFiles(HDROP hDropInfo)
{
    // TODO: Add your message handler code here and/or call default
    int nCntFiles = DragQueryFile(hDropInfo, 0xFFFFFFFF, 0, 0);


    std::deque<fs::path> localpaths;

    std::string megaprefix, initialmegaprefix;
    m::MegaApi::utf8ToUtf16(PlatformMegaUNCPrefix(nullptr).c_str(), &initialmegaprefix);
    std::deque<std::string> megapaths;
    OwningApiPtr source_masp;

    for (int j = 0; j < nCntFiles; j++)
    {
        UINT bufsize = ::DragQueryFileW(hDropInfo, j, NULL, 0) + 1;
        unique_ptr<wchar_t[]> buf(new wchar_t[bufsize]);

        if (::DragQueryFileW(hDropInfo, j, buf.get(), bufsize))
        {
            buf[bufsize - 1] = 0;
            if (memcmp(buf.get(), initialmegaprefix.data(), initialmegaprefix.size()/2-1))
            {
                fs::path p(buf.get());
                error_code e;
                auto s = fs::status(p, e);
                if (!e && (fs::is_regular_file(s) || fs::is_directory(s)))
                {
                    localpaths.push_back(p);
                }
                else if (e)
                {
                    ReportError("Sorry, Can't accept files due to error " + e.message() + " for path " + p.u8string());
                    return;
                }
                else
                {
                    ReportError("Sorry, Non-file non-folder path " + p.u8string());
                    return;
                }
            }
            else
            {
                if (source_masp && !memcmp(buf.get(), megaprefix.data(), megaprefix.size()/2))
                {
                    wstring ws(buf.get() + megaprefix.size()/2, bufsize - 1 - megaprefix.size()/2);
                    string s;
                    m::MegaApi::utf16ToUtf8(ws.data(), (int)ws.size(), &s);
                    megapaths.push_back(move(s));
                }
                else if (source_masp)
                {
                    ReportError("Sorry, can't accept paths from multiple MEGA sources");
                    return;
                }
                else 
                {
                    std::wstring s(buf.get() + initialmegaprefix.size()/2, bufsize - 1 - initialmegaprefix.size()/2);
                    auto pos = s.find(L"\\");
                    if (pos + 1)
                    { 
                        s.erase(pos);
                        source_masp = g_mega->findMegaApi(wcstoull(s.c_str(), nullptr, 10));
                        if (!source_masp)
                        {
                            ReportError("MEGA source from path not found in this instance");
                            return;
                        }
                        else
                        {
                            m::MegaApi::utf8ToUtf16(PlatformMegaUNCPrefix(source_masp.get()).c_str(), &megaprefix);
                            wstring ws(buf.get() + megaprefix.size()/2, bufsize - 1 - megaprefix.size()/2);
                            string s;
                            m::MegaApi::utf16ToUtf8(ws.data(), (int)ws.size(), &s);
                            megapaths.push_back(move(s));
                        }
                    }
                    else
                    {
                        ReportError("Badly formatted MEGA path prefix");
                        return;
                    }
                }
            }
        }
    }

    if (localpaths.size() && megapaths.empty())
    {
        activeReader->OnDragDroppedLocalItems(localpaths);
    }
    else if (localpaths.empty() && megapaths.size())
    {
        deque<unique_ptr<m::MegaNode>> nodes;
        for (auto& p : megapaths)
        {
            unique_ptr<m::MegaNode> np(source_masp->getNodeByPath(p.c_str()));
            if (!np)
            {
                ReportError("Could not find path in MEGA: " + p);
                return;
            }
            nodes.emplace_back(std::move(np));
        }

        activeReader->OnDragDroppedMEGAItems(source_masp, nodes);
    }
    else
    {
        ReportError("Sorry, can't handle mixed MEGA and local paths simultaneously");
    }
}

void CFilesPPDlg::SavePlaylist()
{
    fs::path path;
    if (auto playlist = dynamic_cast<MetaPath_Playlist*>(m_pathCtl.metaPath.get()))
    {
        ofstream outfile(playlist->localPath);
        outfile << "[";
        int comma = 0;
        items.for_each([&comma, &outfile](std::unique_ptr<Item>& i){
            if (auto p = dynamic_cast<ItemMegaNode*>(i.get()))
            {
                if (comma++) { outfile << ","; }
                outfile << "{\"h\":\"";
                outfile << OwnStr(p->mnode->getBase64Handle(), true);
                outfile << "\"}";
            }
        });
        outfile << "]";
        outfile.flush();
        if (outfile)
        {
            ReportError("Playlist saved successfully");
            return;
        }
    }
    ReportError("Could not save Playlist");
}

enum { MENU_COPYNAMES = 1, MENU_COPYPATHS, MENU_COPYNAMESQUOTED, MENU_COPYPATHSQUOTED, MENU_SUBSEQUENT };

void CFilesPPDlg::OnNMRClickList2(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMITEMACTIVATE pNMItemActivate = reinterpret_cast<LPNMITEMACTIVATE>(pNMHDR);

    CMenu contextMenu;
    if (contextMenu.CreatePopupMenu())
    {

        auto selectedItems = make_shared<std::deque<Item*>>();
        
        POSITION pos = m_contentCtl.GetFirstSelectedItemPosition();
        while (pos)
        {
            unsigned nItem = (unsigned)m_contentCtl.GetNextSelectedItem(pos);
            if (nItem < filteredItems.size())
            {
                selectedItems->push_back(filteredItems[nItem]);
            }
        }

        contextMenu.AppendMenu(MF_STRING, MENU_COPYNAMES, _T("Copy Names"));
        contextMenu.AppendMenu(MF_STRING, MENU_COPYPATHS, _T("Copy Full Paths"));
        contextMenu.AppendMenu(MF_STRING, MENU_COPYNAMESQUOTED, _T("Copy Names Quoted"));
        contextMenu.AppendMenu(MF_STRING, MENU_COPYPATHSQUOTED, _T("Copy Full Paths Quoted"));

        map<int, std::function<void()>> menuexec;

        MenuActions ma;
        if (activeReader) ma = activeReader->GetMenuActions(selectedItems, *m_pathCtl.metaPath);
        if (dynamic_cast<MetaPath_Playlist*>(m_pathCtl.metaPath.get()))
        {
            ma.actions.emplace_back("Save Playlist", [this]()
            {
                SavePlaylist();
            });
        }


        ClientToScreen(&pNMItemActivate->ptAction);
        auto result = ExecMenu(contextMenu, ma, MENU_SUBSEQUENT, pNMItemActivate->ptAction, this);

        if (result)
        {
            string copyString;
            int lines = 0;

            for (auto& item : *selectedItems)
            {
                string fullpath = m_pathCtl.metaPath->GetFullPath(*item);

                switch (result)
                {
                case MENU_COPYNAMES: ++lines; copyString += item->u8Name + "\r\n";  break;
                case MENU_COPYPATHS: ++lines; copyString += fullpath + "\r\n"; break;
                case MENU_COPYNAMESQUOTED: ++lines; copyString += "\"" + item->u8Name + "\"\r\n";  break;
                case MENU_COPYPATHSQUOTED: ++lines; copyString += "\"" + fullpath + "\"\r\n"; break;
                }
            }

            if (result == MENU_COPYNAMES || result == MENU_COPYPATHS || result == MENU_COPYNAMESQUOTED || result == MENU_COPYPATHSQUOTED)
            {
                bool copySucceded = PutStringToClipboard(copyString);

                if (copySucceded) SetUserFeedback(ostringstream() << "Copied " << lines << " lines to the clipboard");
                else SetUserFeedback(ostringstream() << "Copy to clipboard failed");
            }
        }
    }

    *pResult = 0;
}

void CFilesPPDlg::SetUserFeedbackCStr(const char* s)
{
    m_statusBar.SetText(CA2CT(s), 255, SBT_NOBORDERS);
}

void CFilesPPDlg::ClearFilteredItems()
{
    filteredItems.clear();
}

void CFilesPPDlg::AddFilteredItem(Item* p)
{
    //if (activeReader->NewAtTop())
    filteredItems.add(move(p));
}

void CFilesPPDlg::DoneAddingFilteredItems()
{
    m_contentCtl.SetItemCount(int(filteredItems.size()));
}

void CFilesPPDlg::SetFilterText(const string& s)
{
    m_filterButton.SetWindowText(CA2CT(s.c_str()));
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
    wstring filenamelist;

    POSITION pos = m_contentCtl.GetFirstSelectedItemPosition();
    while (pos)
    {
        unsigned nItem = (unsigned)m_contentCtl.GetNextSelectedItem(pos);
        if (nItem < filteredItems.size())
        {
            std::string uncPath;
            if (m_pathCtl.metaPath->GetDragDropUNCPath(filteredItems[nItem], uncPath))
            {
                string u16string;
                m::MegaApi::utf8ToUtf16(uncPath.c_str(), &u16string);
                filenamelist.append(reinterpret_cast<wchar_t*>(u16string.data()), u16string.size() / 2);
                filenamelist.append(1, 0);
            }
            else
                return; // if we can't drag/drop all, don't mislead the user by making it look like we can.
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


void CFilesPPDlg::OnDeltaposSpin1(NMHDR *pNMHDR, LRESULT *pResult)
{
    LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);

    if (pNMUpDown->iDelta > 0)
    {
        if (g_mega->favourites.Toggle(*m_pathCtl.metaPath))
        {
            AfxMessageBox(_T("Added favourite"));
        }
        else
        {
            AfxMessageBox(_T("Removed favourite"));
        }
        g_mega->saveFavourites(m_pathCtl.metaPath->Account());
    }
    else if (pNMUpDown->iDelta < 0)
    {
        CMenu m;
        m.CreatePopupMenu();
        auto favourites = g_mega->favourites.copy();
        OwningAccountPtr currAcc = nullptr;
        MenuActions ma;
        for (unsigned i = 0; i < favourites.size(); ++i)
        {
            auto test = favourites[i]->Account();
            if (test != currAcc)
            {
                currAcc = test;
                ma.actions.push_back(MenuActions::MenuItem{ currAcc ? OwnString(currAcc->masp->getMyEmail()) : string("<Local>") , nullptr, true });
            }
            ma.actions.push_back(MenuActions::MenuItem{ favourites[i]->u8DisplayPath(), [thisfavourite = std::shared_ptr<MetaPath>(favourites[i]->clone()), this]()
                {
                    if (GetKeyState(VK_SHIFT) & 0x8000)
                    {
                        // make a new top level window (complete with its own gui thread) to load subdir 
                        auto t = make_unique<CFilesPPDlgThread>();
                        t->dlg.m_pathCtl.metaPath = thisfavourite->clone();
                        t->m_bAutoDelete = true;
                        GetWindowRect(t->dlg.originatorWindowRect);
                        if (!t->CreateThread()) AfxMessageBox(_T("Thread creation failed"));
                        else t.release();
                    }
                    else
                    {
                        m_pathCtl.PrepareNavigateBack();
                        m_pathCtl.metaPath = thisfavourite->clone();
                        LoadContent(true);
                    }

                } });
        }
        CRect r;
        m_spinCtrl.GetWindowRect(r);
        ExecMenu(m, ma, 100, POINT{ r.left, r.bottom }, this);
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
            if (nItem < filteredItems.size())
            {
                Item& item = *filteredItems[nItem];
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
