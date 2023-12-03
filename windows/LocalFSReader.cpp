// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "stdafx.h"
#include "LocalFSReader.h"
#include <WinIoCtl.h>
#include "../core/PlatformSupplied.h"

#include "../core/metapath.h"


using namespace std;

LocalVolumeReader::LocalVolumeReader(QueueTrigger t, bool r, UserFeedback& uf)
    : FSReader(t, r, uf)
    , workerthread([this]() { Threaded(); })
{
    columnDefs.push_back(ColumnDef{"Name", 200});
    columnDefs.push_back(ColumnDef{"Size", 50, [](const Item* i){ return to_string(i->size()); }});
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
            string n;
            m::MegaApi::utf16ToUtf8(s.data(), (int)len, &n);

            ULARGE_INTEGER freebytesforcaller, totalbytes, freebytes;
            GetDiskFreeSpaceExA(n.c_str(), &freebytesforcaller, &totalbytes, &freebytes);

            itemQueue->Queue(NEWITEM, make_unique<ItemLocalFS>(n, totalbytes.QuadPart, true));
        }
        s.erase(0, len2 + 1);
    }
    itemQueue->Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
    itemQueue->Send();
}

LocalFSReader::LocalFSReader(fs::path p, bool normalizeSep, QueueTrigger t, bool r, UserFeedback& uf)
    : FSReader(t, r, uf)
    , dir(p)
    , normalizeSeparator(normalizeSep)
{
    memset(&overlapped, 0, sizeof(overlapped));
    workerthread = std::thread([this]() { Threaded(); });

    columnDefs.push_back(ColumnDef{"Name", 200, [](const Item* i){ return i->u8Name + (i->isFolder() ? "\\" : ""); }});
    columnDefs.push_back(ColumnDef{"Size", 50, [](const Item* i){ return i->size() < 0 ? "" : to_string(i->size()); }});
}

LocalFSReader::~LocalFSReader()
{
    cancelling = true;
    if (hDirectory != INVALID_HANDLE_VALUE) CancelIoEx(hDirectory, &overlapped);
    workerthread.join();
}

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

auto LocalFSReader::GetMenuActions(shared_ptr<deque<Item*>> selectedItems, const MetaPath& metapath) -> MenuActions
{
    MenuActions ma;
    ma.actions.emplace_back("Explore To", [this, selectedItems]() { for (auto i : *selectedItems) ExploreTo(hwnd, dir / i->u8Name); });
    ma.actions.emplace_back("Explorer Properties", [this, selectedItems]() { for (auto i : *selectedItems) ExploreProperties(hwnd, dir / i->u8Name); });
    return ma;
}


bool LocalFSReader::ReadDir(const fs::path& p, bool recurse, const fs::path& recurseprefix)
{
    // read directory now
    try
    {
        for (fs::directory_iterator i(p); i != fs::directory_iterator(); ++i)
        {
            fs::path relativename = recurseprefix / i->path().filename();
            auto str = relativename.u8string();
            if (normalizeSeparator) std::replace(str.begin(), str.end(), '\\', '/');
            if (i->is_directory())
            {
                itemQueue->Queue(NEWITEM, make_unique<ItemLocalFS>(move(str), -1, true));
                if (recurse && !ReadDir(i->path(), true, relativename)) return false;
            }
            else if (i->is_regular_file())
            {
                itemQueue->Queue(NEWITEM, make_unique<ItemLocalFS>(move(str), i->file_size(), false));
            }
        }
        return true;
    }
    catch (std::exception& e)
    {
        itemQueue->Queue(FILE_ACTION_APP_ERROR, unique_ptr<Item>(new ItemError("Error reading directory: " + string(e.what()))));
        return false;
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
    } REPARSE_DATA_BUFFER, * PREPARSE_DATA_BUFFER;

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
    hDirectory = CreateFileW(dir.wstring().c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);
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
                    itemQueue->Queue(FOLDER_RESOLVED_SOFTLINK, make_unique<ItemLocalFS>(dir.u8string(), -1, true));
                }
            }
        }

        if (hDirectory == INVALID_HANDLE_VALUE)
        {
            itemQueue->Queue(FILE_ACTION_APP_ERROR, unique_ptr<Item>(new ItemError("Could not read directory: " + ec.message())));
            itemQueue->Send();
            return;
        }
    }

    // overlapped so we can make a first call now, which initialises its internal buffer, so we don't lose any notifications that occur during initial scan (unless overflow occurs)
    memset(&overlapped, 0, sizeof(overlapped));
    overlappedbuf.resize(48 * 1024);
    bool notifyOk = RequestChanges();

    if (!ReadDir(dir, recurse, "")) return;
    itemQueue->Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
    itemQueue->Send();

    // and scan for changes (including any that occured while reading)
    DWORD dwBytes = 0;
    while (notifyOk && !cancelling && GetOverlappedResultEx(hDirectory, &overlapped, &dwBytes, INFINITE, FALSE))
    {
        char* ptr = (char*)overlappedbuf.data();
        while ((int)dwBytes >= sizeof(FILE_NOTIFY_INFORMATION))
        {
            FILE_NOTIFY_INFORMATION* fni = (FILE_NOTIFY_INFORMATION*)ptr;

            if ((char*)& fni->FileName + fni->FileNameLength - (char*)fni > (int)dwBytes) break;

            fs::path p(wstring(fni->FileName, fni->FileNameLength / sizeof(wchar_t)));

            switch (fni->Action)
            {
            case FILE_ACTION_ADDED:
            {
                error_code ec;
                auto s = fs::directory_entry(dir / p, ec);
                if (!ec && fs::is_directory(s)) itemQueue->Queue(NEWITEM, make_unique<ItemLocalFS>(p.u8string(), -1, true));
                if (!ec && fs::is_regular_file(s)) itemQueue->Queue(NEWITEM, make_unique<ItemLocalFS>(p.u8string(), s.file_size(), false));
                break;
            }
            case FILE_ACTION_REMOVED:
                itemQueue->Queue(DELETEDITEM, make_unique<ItemLocalFS>(p.u8string(), -1, false));
                break;

            case FILE_ACTION_RENAMED_OLD_NAME:
                itemQueue->Queue(RENAMEDFROM, make_unique<ItemLocalFS>(p.u8string(), -1, false));
                break;

            case FILE_ACTION_RENAMED_NEW_NAME:
                itemQueue->Queue(RENAMEDTO, make_unique<ItemLocalFS>(p.u8string(), -1, false));
                break;
            }

            ptr += fni->NextEntryOffset;
            dwBytes -= fni->NextEntryOffset ? fni->NextEntryOffset : dwBytes;
        }
        itemQueue->Send();

        // kick off a new query (notifications in the meantime will have been queued in its internal buffer)
        notifyOk = RequestChanges();
    }

    if (!cancelling && !notifyOk)
    {
        itemQueue->Queue(FILE_ACTION_APP_NOTIFY_FAILURE, NULL);
        itemQueue->Send();
    }
}


void LocalFSReader::OnDragDroppedMEGAItems(OwningApiPtr masp, const deque<unique_ptr<m::MegaNode>>& nodes)
{
    for (auto& n : nodes)
    {
        masp->startDownload(n.get(), (dir / n->getName()).u8string().c_str(), nullptr, nullptr, false, nullptr, m::MegaTransfer::COLLISION_CHECK_ASSUMEDIFFERENT, m::MegaTransfer::COLLISION_RESOLUTION_OVERWRITE, false, nullptr);
    }
    ReportError("Queued " + to_string(nodes.size()) + " paths for download");
}


void LocalFSReader::OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& localpaths)
{
    // local paths dropped on local folder
    std::wstring from, to;
    for (auto& path : localpaths)
    {
        error_code e;
        auto s = fs::status(path, e);
        if (!e && (fs::is_regular_file(s) || fs::is_directory(s)))
        {
            auto str = path.u16string();
            from.append(reinterpret_cast<wchar_t*>(str.data()), str.size());
            from.append(1, '\0');
        }
        else if (e)
        {
            ReportError("Path error: " + e.message() + " for path: " + path.u8string());
            return;
        }
        else
        {
            ReportError("Non file non folder path: " + path.u8string());
            return;
        }
    }

    from.append(2, '\0');
    to.append(dir.wstring());
    to.append(2, '\0');

    SHFILEOPSTRUCT s;
    memset(&s, 0, sizeof(s));
    s.hwnd = hwnd;
    s.wFunc = FO_COPY;
    s.fFlags = FOF_ALLOWUNDO;
    s.pFrom = from.data();
    s.pTo = to.data();

    SHFileOperation(&s);
}
