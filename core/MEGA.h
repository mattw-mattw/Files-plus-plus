// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <functional>

#include "basictypes.h"

#include <megaapi.h>
namespace m = ::mega;

struct MEGA
{
	typedef std::shared_ptr<m::MegaApi> ApiPtr;

	struct Account
	{
		std::filesystem::path cacheFolder;
		ApiPtr masp;
		Account(const std::filesystem::path& p, ApiPtr ap) : cacheFolder(p), masp(ap) {}
	};

	MEGA();
	~MEGA();

	std::vector<ApiPtr> accounts() { std::vector<ApiPtr> v; std::lock_guard g(m); for (auto& a : megaAccounts) v.push_back(a.masp); return v; }

	Account makeTempAccount();
	void add(const Account&);
	void logoutremove(ApiPtr masp);
	void deletecache(ApiPtr masp);

	void onLogin(const m::MegaError* e, const std::shared_ptr<m::MegaApi>& masp);

private:
	std::mutex m;
	std::vector<Account> megaAccounts;
};

extern MEGA* g_mega;

struct NodeUpdateListener : public m::MegaGlobalListener
{
	NotifyQueue<std::unique_ptr<m::MegaNodeList>> nq;

	void onNodesUpdate(m::MegaApi*, m::MegaNodeList* nodes) override
	{
		nq.push(std::unique_ptr<m::MegaNodeList>(nodes ? nodes->copy() : nullptr));
	}
};

struct OneTimeListener : public m::MegaRequestListener
{
	std::function<void(m::MegaRequest* request, m::MegaError* e)> fn;

	OneTimeListener(std::function<void(m::MegaRequest* request, m::MegaError* e)> f) : fn(f) {}

	void onRequestFinish(m::MegaApi*, m::MegaRequest* request, m::MegaError* e) override
	{
		fn(request, e);
		delete this;
	}
};

