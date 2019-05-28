// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <functional>

#include "basictypes.h"

#include <megaapi.h>
namespace m = ::mega;

class MEGA
{
	std::mutex m;
	std::vector<std::shared_ptr<m::MegaApi>> megaAccounts;

public:

	MEGA();

	std::vector<std::shared_ptr<m::MegaApi>> accounts() { std::lock_guard g(m); return megaAccounts; }

	std::pair<std::shared_ptr<m::MegaApi>, std::filesystem::path> makeTempAccount();
	void add(std::shared_ptr<m::MegaApi> p, std::filesystem::path path);

	void onLogin(const m::MegaError& e, const std::shared_ptr<m::MegaApi>& masp);
};

extern MEGA* g_mega;

struct NodeUpdateListener : public m::MegaGlobalListener
{
	NotifyQueue<std::unique_ptr<m::MegaNodeList>> nq;

	void onNodesUpdate(m::MegaApi*, m::MegaNodeList* nodes) override
	{
		nq.push(std::unique_ptr<m::MegaNodeList>(nodes->copy()));
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

