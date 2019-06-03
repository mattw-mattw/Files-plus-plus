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
					megaAccounts.push_back(masp);
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
	string sidvaluestr(sidvalue.get());
	fs::path sidFile = path / "sid";
	std::ofstream sid(path / "sid", ios::binary);
	if (LocalUserCrypt(sidvaluestr, true)) sid << sidvaluestr;
	else ReportError("Failed to encrypt SID");
}
