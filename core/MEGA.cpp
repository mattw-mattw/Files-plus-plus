// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <fstream>

#include "MEGA.h"
#include "PlatformSupplied.h"

using namespace std;

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
			masp->fastLogin(sid.c_str(), new OneTimeListener([this, masp](m::MegaRequest* request, m::MegaError* e) { onLogin(*e, masp); }));
		}
	}
}

void MEGA::onLogin(const m::MegaError& e, const shared_ptr<m::MegaApi>& masp)
{
	if (e.getErrorCode())
	{
		ReportError("MEGA Login Failed: ", e);
	}
	else
	{
		masp->fetchNodes(new OneTimeListener([](m::MegaRequest* request, m::MegaError* e)
			{
				if (e->getErrorCode()) ReportError("MEGA account FetchNodes failed: ", *e);
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
