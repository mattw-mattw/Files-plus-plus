// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "stdafx.h"
#include "../core/PlatformSupplied.h"
#include "UserPassDialog.h"
#include "LocalFSReader.h"
#include <wincrypt.h>

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


void AddMEGAAccount()
{
	MEGA::AccountPtr acc = g_mega->makeTempAccount();

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
				g_mega->onLogin(nullptr, acc->masp);
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
    MEGA::FolderPtr acc = g_mega->makeTempFolder();

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
                g_mega->onLogin(nullptr, acc->masp);
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


unique_ptr<FSReader> NewLocalFSReader(const fs::path& localPath, FSReader::QueueTrigger t, bool recurse)
{
	return make_unique<LocalFSReader>(localPath, t, recurse);
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
