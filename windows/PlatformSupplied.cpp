// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "stdafx.h"
#include <wincrypt.h>
#include <winuser.h>

#include "../core/PlatformSupplied.h"
#include "../core/MEGA.h"
#include "UserPassDialog.h"
#include "TextDialog.h"
#include "LocalFSReader.h"

using namespace std;

fs::path BasePath()
{
	TCHAR szPath[MAX_PATH];
	if (!SUCCEEDED(SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, szPath)))
	{
		AfxMessageBox(_T("Could not get local appdata path"));
		return fs::path();
	}
	fs::path p(szPath);
	auto basepath = p / fs::path("Files++");
	fs::create_directories(basepath);
	return basepath;
}


void ReportError(const std::string& message, const m::MegaError* e)
{
    AfxMessageBox(CA2CT((message + (e ? string(e->getErrorString()) + " (" + to_string(e->getErrorCode()) + ")" : string())).c_str()));
}

void ReportError(const std::string& message, const c::MegaChatError* e)
{
    AfxMessageBox(CA2CT((message + (e ? string(e->getErrorString()) + " (" + to_string(e->getErrorCode()) + ")" : string())).c_str()));
}

bool QueryUserOkCancel(const std::string& message)
{
    return IDOK == AfxMessageBox(CA2CT(message.c_str()), MB_OKCANCEL);
}

void AddMEGAAccount()
{
	OwningAccountPtr acc = g_mega->makeTempAccount();

	UserPassDialog dlg;
	for (;;)
	{
		if (IDOK == dlg.DoModal())
		{
			bool done = false, success = false;
			acc->masp->login(CT2CA(dlg.name), CT2CA(dlg.password), new OneTimeListener([&](m::MegaRequest*, m::MegaError* e) { success = e->getErrorCode() == m::MegaError::API_OK; done = true;  }));
			while (!done) Sleep(100);
			if (success)
			{
				g_mega->addAccount(acc);
				AfxMessageBox(_T("Login succeeded"));
				g_mega->onLogin(nullptr, acc);
				return;
			}
			AfxMessageBox(_T("Login failed"));
			continue;
		}
		else break;
	}
	std::error_code ec;
	fs::remove_all(acc->cacheFolder, ec);
}

void AddMEGAFolderLink()
{
    OwningFolderPtr acc = g_mega->makeTempFolder();

    UserPassDialog dlg;
    for (;;)
    {
        if (IDOK == dlg.DoModal())
        {
            bool done = false, success = false;
            acc->masp->loginToFolder(CT2CA(dlg.name), new OneTimeListener([&](m::MegaRequest*, m::MegaError* e) { success = e->getErrorCode() == m::MegaError::API_OK; done = true;  }));
            while (!done) Sleep(100);
            if (success)
            {
                acc->folderLink = CT2CA(dlg.name);
                g_mega->addFolder(acc);
                AfxMessageBox(_T("Opened Folder Ok"));
                g_mega->onLogin(nullptr, acc);
                return;
            }
            AfxMessageBox(_T("Folder link failed to open"));
            continue;
        }
        else break;
    }
    std::error_code ec;
    fs::remove_all(acc->cacheFolder, ec);
}


unique_ptr<FSReader> NewLocalFSReader(const fs::path& localPath, QueueTrigger t, bool recurse, UserFeedback& uf)
{
	return make_unique<LocalFSReader>(localPath, t, recurse, uf);
}

bool LocalUserCrypt(string& data, bool encrypt)
{
	DATA_BLOB out{ 0, nullptr };
	DATA_BLOB in{ DWORD(data.size()), (BYTE*)data.data() };
	DATA_BLOB entropy{ 7, (BYTE*)"Files++" };
	bool success = encrypt ?
		CryptProtectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out) :
		CryptUnprotectData(&in, nullptr, nullptr, nullptr, nullptr, 0, &out);
	if (success)
	{
		data.assign((char*)out.pbData, out.cbData);
		LocalFree(out.pbData);
	}
	return success;
}

std::string PlatformLocalUNCPrefix()
{
    //return "\\\\.\\";
    return string();  // disappointing: windows explorer doesn't seem to accept UNC paths
}

std::string PlatformMegaUNCPrefix(m::MegaApi* mp)
{
    if (mp)  return "\\\\Files++:" + to_string(reinterpret_cast<uint64_t>(mp)) + "\\";
    else return "\\\\Files++:";
}

bool PutStringToClipboard(const string& copyString)
{
    string u16string;
    m::MegaApi::utf8ToUtf16(copyString.c_str(), &u16string);
    u16string.append(2, '\0');

    bool copySucceeded = false;
    for (auto start = chrono::steady_clock::now(); chrono::steady_clock::now() - start < 1s; Sleep(1))
    {
        if (OpenClipboard(NULL))
        {
            if (EmptyClipboard())
            {
                auto len = u16string.size();
                if (HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, len))
                {
                    auto gptr = GlobalLock(hg);
                    memcpy(gptr, u16string.data(), len);
                    GlobalUnlock(hg);
                    auto b = SetClipboardData(CF_UNICODETEXT, hg);
                    copySucceeded = b == hg;
                }
            }
            CloseClipboard();
            break;
        }
    }
    return copySucceeded;
}

void WaitMillisec(unsigned n)
{
    Sleep(n);
}

bool InputUserChatMessage(std::string& msg)
{
    TextDialog dlg;
    if (IDOK == dlg.DoModal())
    {
        msg = CT2CA(dlg.textString);
        return true;
    }
    return false;
}