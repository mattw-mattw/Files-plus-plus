// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "stdafx.h"
#include "LocalFSReader.h"
#include <WinIoCtl.h>

using namespace std;

LocalVolumeReader::LocalVolumeReader(QueueTrigger t, bool r)
	: FSReader(t, r)
	, workerthread([this]() { Threaded(); })
{
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

LocalFSReader::LocalFSReader(fs::path p, QueueTrigger t, bool r)
	: FSReader(t, r)
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
	catch (std::exception&)
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

			if ((char*)& fni->FileName + fni->FileNameLength - (char*)fni > (int)dwBytes) break;

			fs::path p(wstring(fni->FileName, fni->FileNameLength / sizeof(wchar_t)));

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
