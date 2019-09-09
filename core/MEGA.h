// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <mutex>
#include <vector>
#include <memory>
#include <functional>
#include <atomic>
#include <condition_variable>

#include "basictypes.h"
#include "MetaPath.h"


struct OneTimeListener : public m::MegaRequestListener
{
    std::function<void(m::MegaRequest* request, m::MegaError* e)> fn;

    OneTimeListener(std::function<void(m::MegaRequest* request, m::MegaError* e)> f) : fn(f) {}

    void onRequestFinish(m::MegaApi*, m::MegaRequest* request, m::MegaError* e) override
    {
        if (fn) fn(request, e);
        delete this;
    }
};

class MRequest : public OneTimeListener, public std::enable_shared_from_this<MRequest>
{
    ApiPtr mawp;
    std::mutex m;
    std::string action;
    std::string state;
    bool finished = false;

    void onRequestStart(m::MegaApi* api, m::MegaRequest* request)
    {
        std::lock_guard g(m);
        state += "started";
    }
    void onRequestFinish(m::MegaApi* api, m::MegaRequest* request, m::MegaError* e)
    {
        std::lock_guard g(m);
        state += (!e || e->getErrorCode() == m::MegaError::API_OK) ? " succeeded" : ("failed: " + std::string(e->getErrorString()));
        finished = true;
        if (fn) fn(request, e);
    }
    void onRequestUpdate(m::MegaApi* api, m::MegaRequest* request)
    {
    }
    void onRequestTemporaryError(m::MegaApi* api, m::MegaRequest* request, m::MegaError* error)
    {
        std::lock_guard g(m);
        state += " temperr";
    }

public:
    MRequest(const OwningApiPtr& ap, const std::string& a);

    MRequest(const OwningApiPtr& ap, const std::string& a, std::function<void(m::MegaRequest* request, m::MegaError* e)> f);

    std::string getAction()
    {
        std::lock_guard g(m);
        return action;
    }
    std::string getState()
    {
        std::lock_guard g(m);
        return state;
    }
};


struct MEGA
{
	MEGA();
	~MEGA();

    std::vector<AccountPtr> accounts() { std::lock_guard g(m); return megaAccounts; }
    std::vector<FolderPtr> folderLinks() { std::lock_guard g(m); return megaFolderLinks; }

    AccountPtr getAccount(ApiPtr a) { std::lock_guard g(m); for (auto& p : megaAccounts) if (p->masp == a.lock()) return p; return {}; }

    std::atomic<int> accountsUpdateCount = 0;
    std::atomic<int> folderLinksUpdateCount = 0;

    std::condition_variable updateCV;
    std::mutex updateCVMutex;

    AccountPtr makeTempAccount();
    FolderPtr makeTempFolder();
    void addAccount(AccountPtr&);
    void addFolder(FolderPtr&);
    void logoutremove(OwningApiPtr masp);
	void deletecache(OwningApiPtr masp);

    OwningApiPtr findMegaApi(uint64_t dragdroptoken);

    void AddMRequest(ApiPtr masp, std::shared_ptr<MRequest>&&);
    std::deque<std::shared_ptr<MRequest>> getRequestHistory();

	void onLogin(const m::MegaError* e, AccountPtr masp);

    Favourites favourites;
    void loadFavourites(AccountPtr macc);
    void saveFavourites(AccountPtr macc);


    static std::string ToBase64(const std::string& s);
    static std::string FromBase64(const std::string& s);

private:
	std::mutex m;
	std::vector<AccountPtr> megaAccounts;
    std::vector<FolderPtr> megaFolderLinks;
    std::deque<std::shared_ptr<MRequest>> requestHistory;
};

extern MEGA* g_mega;



