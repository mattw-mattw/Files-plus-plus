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
				string sid;
				ifstream f(d->path() / "sid", ios::binary);
				char c;
				while (f.get(c)) sid += c;
				if (!sid.empty() && LocalUserCrypt(sid, false))
				{
					auto masp = make_shared<m::MegaApi>("BTgWXaYb", d->path().u8string().c_str(), "Files++");
					megaAccounts.emplace_back(d->path(), masp);
					masp->fastLogin(sid.c_str(), new OneTimeListener([this, masp](m::MegaRequest* request, m::MegaError* e) { onLogin(e, masp); }));
				}
				else
				{
					ReportError("Failed to retrieve SID for account in folder " + d->path().u8string());
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
	for (auto& a : megaAccounts)
	{
		a.masp.reset();  // should be deleted at this point
	}
}

void MEGA::onLogin(const m::MegaError* e, const shared_ptr<m::MegaApi>& masp)
{
	if (e && e->getErrorCode())
	{
		ReportError("MEGA Login Failed: ", e);
	}
	else
	{
		masp->fetchNodes(new OneTimeListener([](m::MegaRequest* request, m::MegaError* e)
			{
				if (e && e->getErrorCode()) ReportError("MEGA account FetchNodes failed: ", e);
			}));
	}
}

MEGA::Account MEGA::makeTempAccount()
{
	fs::path p = BasePath() / to_string(time(NULL));
	fs::create_directories(p);
	return Account(p, make_shared<m::MegaApi>("BTgWXaYb", p.u8string().c_str(), "Files++"));
}

void MEGA::add(const Account& a) {
	std::lock_guard g(m);
	unique_ptr<char[]> sidvalue(a.masp->dumpSession());
	string sidvaluestr(sidvalue.get());
	fs::path sidFile = a.cacheFolder / "sid";
	std::ofstream sid(sidFile, ios::binary);
	if (LocalUserCrypt(sidvaluestr, true))
	{
		if (sid << sidvaluestr << flush)
		{
			megaAccounts.push_back(a);
			return;
		}
	}
	sid.close();
	std::error_code ec;
	fs::remove_all(a.cacheFolder, ec);
	ReportError("Failed to encrypt or write SID");
}

void MEGA::logoutremove(std::shared_ptr<m::MegaApi> masp)
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

void MEGA::deletecache(std::shared_ptr<m::MegaApi> masp)
{
	std::lock_guard g(m);
	for (auto it = megaAccounts.begin(); it != megaAccounts.end(); ++it)
	{
		if (it->masp == masp)
		{
			std::error_code ec;
			fs::remove_all(it->cacheFolder, ec);
			if (ec)
			{
				ReportError("Could not delete cache folder: " + ec.message());
			}
			else
			{
				megaAccounts.erase(it);
				return;
			}
		}
	}
}