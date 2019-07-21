// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <condition_variable>

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

    struct Folder : Account
    {
        std::string folderLink;
        Folder(const std::string& link, const std::filesystem::path& p, ApiPtr ap) : Account(p, ap), folderLink(link) {}
        ~Folder() {}
    };

    typedef std::shared_ptr<Account> AccountPtr;
    typedef std::shared_ptr<Folder> FolderPtr;


	MEGA();
	~MEGA();

    std::vector<AccountPtr> accounts() { std::lock_guard g(m); return megaAccounts; }
    std::vector<FolderPtr> folderLinks() { std::lock_guard g(m); return megaFolderLinks; }

    std::atomic<int> accountsUpdateCount = 0;
    std::atomic<int> folderLinksUpdateCount = 0;

    std::condition_variable updateCV;
    std::mutex updateCVMutex;

    AccountPtr makeTempAccount();
    FolderPtr makeTempFolder();
    void addAccount(AccountPtr&);
    void addFolder(FolderPtr&);
    void logoutremove(ApiPtr masp);
	void deletecache(ApiPtr masp);

    ApiPtr findMegaApi(uint64_t dragdroptoken);

	void onLogin(const m::MegaError* e, const ApiPtr& masp);

private:
	std::mutex m;
	std::vector<AccountPtr> megaAccounts;
    std::vector<FolderPtr> megaFolderLinks;
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

