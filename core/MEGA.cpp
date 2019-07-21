// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <fstream>

#include "MEGA.h"
#include "PlatformSupplied.h"

using namespace std;

MEGA::MEGA()
{
	fs::path base = BasePath();
	try
	{
		for (auto d = fs::directory_iterator(base); d != fs::directory_iterator(); ++d)
		{
			if (d->path().filename().u8string().find("-failed") == string::npos)
			{
				string sid, link;
                ifstream f(d->path() / "sid", ios::binary);
                ifstream f2(d->path() / "link", ios::binary);
                char c;
                while (f.get(c)) sid += c;
                while (f2.get(c)) link += c;
                if (!sid.empty() && LocalUserCrypt(sid, false))
                {
                    auto masp = make_shared<m::MegaApi>("BTgWXaYb", d->path().u8string().c_str(), "Files++");
                    megaAccounts.push_back(make_shared<Account>(d->path(), masp));
                    masp->fastLogin(sid.c_str(), new OneTimeListener([this, masp](m::MegaRequest* request, m::MegaError* e) { onLogin(e, masp); }));
                }
                else if (!link.empty() && LocalUserCrypt(link, false))
                {
                    auto masp = make_shared<m::MegaApi>("BTgWXaYb", d->path().u8string().c_str(), "Files++");
                    megaFolderLinks.push_back(make_shared<Folder>(link, d->path(), masp));
                    masp->loginToFolder(link.c_str(), new OneTimeListener([this, masp](m::MegaRequest* request, m::MegaError* e) { onLogin(e, masp); }));
                }
                else
				{
					ReportError("Failed to retrieve SID or Link for account/link in folder " + d->path().u8string());
					fs::path newpath = d->path(); newpath += "-failed";
					f.close();
					std::error_code err;
					fs::rename(d->path(), newpath, err);
				}
			}
		}
	}
	catch (std::exception& e)
	{
		ReportError(e.what());
	}
}

MEGA::~MEGA()
{
	std::lock_guard g(m);
    for (auto& a : megaAccounts) { a->masp->localLogout(); }
    for (auto& a : megaFolderLinks) { a->masp->localLogout(); }
}

auto MEGA::findMegaApi(uint64_t dragdroptoken) -> ApiPtr
{
    std::lock_guard g(m);
    for (auto& a : megaAccounts) { if (reinterpret_cast<uint64_t>(a->masp.get()) == dragdroptoken) return a->masp; }
    for (auto& a : megaFolderLinks) { if (reinterpret_cast<uint64_t>(a->masp.get()) == dragdroptoken) return a->masp; }
    return nullptr;
}

void MEGA::onLogin(const m::MegaError* e, const shared_ptr<m::MegaApi>& masp)
{
	if (e && e->getErrorCode())
	{
		ReportError("MEGA Login Failed: ", e);
	}
	else
	{
		masp->fetchNodes(new OneTimeListener([this](m::MegaRequest* request, m::MegaError* e)
			{
				if (e && e->getErrorCode()) ReportError("MEGA account FetchNodes failed: ", e);
                ++accountsUpdateCount;
                ++folderLinksUpdateCount;
                updateCV.notify_all();
			}));
	}
    ++accountsUpdateCount;
    ++folderLinksUpdateCount;
    updateCV.notify_all();
}

MEGA::AccountPtr MEGA::makeTempAccount()
{
    fs::path p = BasePath() / to_string(time(NULL));
    fs::create_directories(p);
    return make_shared<Account>(p, make_shared<m::MegaApi>("BTgWXaYb", p.u8string().c_str(), "Files++"));
}

MEGA::FolderPtr MEGA::makeTempFolder()
{
    fs::path p = BasePath() / to_string(time(NULL));
    fs::create_directories(p);
    return make_shared<Folder>("", p, make_shared<m::MegaApi>("BTgWXaYb", p.u8string().c_str(), "Files++"));
}

void MEGA::addAccount(AccountPtr& a) {
    std::lock_guard g(m);
    unique_ptr<char[]> sidvalue(a->masp->dumpSession());
    string sidvaluestr(sidvalue.get());
    fs::path sidFile = a->cacheFolder / "sid";
    std::ofstream sid(sidFile, ios::binary);
    if (LocalUserCrypt(sidvaluestr, true))
    {
        if (sid << sidvaluestr << flush)
        {
            megaAccounts.push_back(a);
            ++accountsUpdateCount;
            updateCV.notify_all();
            return;
        }
    }
    sid.close();
    std::error_code ec;
    fs::remove_all(a->cacheFolder, ec);
    ReportError("Failed to encrypt or write SID");
}

void MEGA::addFolder(FolderPtr& a) {
    std::lock_guard g(m);
    string sidvaluestr(a->folderLink);
    fs::path sidFile = a->cacheFolder / "link";
    std::ofstream sid(sidFile, ios::binary);
    if (LocalUserCrypt(sidvaluestr, true))
    {
        if (sid << sidvaluestr << flush)
        {
            megaFolderLinks.push_back(a);
            ++folderLinksUpdateCount;
            updateCV.notify_all();
            return;
        }
    }
    sid.close();
    std::error_code ec;
    fs::remove_all(a->cacheFolder, ec);
    ReportError("Failed to encrypt or write folder link");
}

void MEGA::logoutremove(ApiPtr masp)
{
	if (masp->isLoggedIn())
	{
		masp->logout(new OneTimeListener([this, masp](m::MegaRequest*, m::MegaError* e) {
			if (e && e->getErrorCode() != m::MegaError::API_OK) ReportError("Logout error: ", e);
			deletecache(masp);
			}));
	}
	else
	{
		deletecache(masp);
	}
}

void MEGA::deletecache(ApiPtr masp)
{
	std::lock_guard g(m);
    for (auto it = megaAccounts.begin(); it != megaAccounts.end(); ++it)
    {
        if ((*it)->masp == masp)
        {
            (*it)->masp->localLogout();
            std::error_code ec;
            fs::remove_all((*it)->cacheFolder, ec);
            if (ec) ReportError("Could not delete cache folder: " + ec.message());
            megaAccounts.erase(it);
            ++accountsUpdateCount;
            updateCV.notify_all();
            return;
        }
    }
    for (auto it = megaFolderLinks.begin(); it != megaFolderLinks.end(); ++it)
    {
        if ((*it)->masp == masp)
        {
            (*it)->masp->localLogout();
            std::error_code ec;
            fs::remove_all((*it)->cacheFolder, ec);
            if (ec) ReportError("Could not delete cache folder: " + ec.message());
            megaFolderLinks.erase(it);
            ++folderLinksUpdateCount;
            updateCV.notify_all();
            return;
        }
    }
}