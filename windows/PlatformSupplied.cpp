// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "stdafx.h"
#include "../core/PlatformSupplied.h"
#include "UserPassDialog.h"
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


void ReportError(const std::string& message, const m::MegaError& e)
{
	AfxMessageBox(CA2CT((message + e.getErrorString() + " (" + to_string(e.getErrorCode()) + ")").c_str()));
}


void AddMEGAAccount()
{
	auto [masp, path] = g_mega->makeTempAccount();

	UserPassDialog dlg;
	for (;;)
	{
		if (IDOK == dlg.DoModal())
		{
			bool done = false, success = false;
			masp->login(CT2CA(dlg.name), CT2CA(dlg.password), new OneTimeListener([&](m::MegaRequest*, m::MegaError* e) { success = e->getErrorCode() == m::MegaError::API_OK; done = true;  }));
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

unique_ptr<FSReader> NewLocalFSReader(const fs::path& localPath, FSReader::QueueTrigger t, bool recurse)
{
	return make_unique<LocalFSReader>(localPath, t, recurse);
}
