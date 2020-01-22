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


struct OneTimeChatListener : public c::MegaChatRequestListener
{
    std::function<void(c::MegaChatRequest * request, c::MegaChatError * e)> fn;

    OneTimeChatListener(std::function<void(c::MegaChatRequest * request, c::MegaChatError * e)> f) : fn(f) {}

    void onRequestFinish(c::MegaChatApi* api, c::MegaChatRequest* request, c::MegaChatError* e) override
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

struct Users
{
    std::string Firstname(m::MegaHandle h, m::MegaApi& ma)
    {
        std::lock_guard g(m);
        user& u = users[h];
        if (u.requestedFirstname) return u.firstname;

        auto i = contacts.find(h);
        if (i == contacts.end())
        {
            std::unique_ptr<m::MegaUserList> c(ma.getContacts());
            for (int i = c->size(); i--; ) contacts[c->get(i)->getHandle()].reset(c->get(i)->copy());
            i = contacts.find(h);
        }
        if (i != contacts.end())
        {
            u.requestedFirstname = true;
            ma.getUserAttribute(i->second.get(), m::MegaApi::USER_ATTR_FIRSTNAME, new OneTimeListener([this, h](m::MegaRequest* request, m::MegaError* e){
                std::lock_guard g(m);
                if (e && !e->getErrorCode()) users[h].firstname = request->getText();
            }));
        }

        return OwnString(m::MegaApi::handleToBase64(h));
    }

private:
    struct user
    {
        std::string firstname;
        std::string email;

        bool requestedFirstname = false;
        bool requestedEmail = false;
    };
    std::mutex m;
    std::map<m::MegaHandle, user> users;
    std::map<m::MegaHandle, std::unique_ptr<m::MegaUser>> contacts;
};


struct MEGA
{
	MEGA();
	~MEGA();

    std::vector<OwningAccountPtr> accounts() { std::lock_guard g(m); return megaAccounts; }
    std::vector<OwningFolderPtr> folderLinks() { std::lock_guard g(m); return megaFolderLinks; }

    OwningAccountPtr getAccount(ApiPtr a) { std::lock_guard g(m); for (auto& p : megaAccounts) if (p->masp == a.lock()) return p; return {}; }

    std::atomic<int> accountsUpdateCount = 0;
    std::atomic<int> folderLinksUpdateCount = 0;

    std::condition_variable updateCV;
    std::mutex updateCVMutex;

    OwningAccountPtr makeTempAccount();
    OwningFolderPtr makeTempFolder();
    void addAccount(OwningAccountPtr&);
    void addFolder(OwningFolderPtr&);
    void logoutremove(OwningAccountPtr masp);
	void deletecache(OwningAccountPtr masp);

    OwningApiPtr findMegaApi(uint64_t dragdroptoken);

    void AddMRequest(ApiPtr masp, std::shared_ptr<MRequest>&&);
    std::deque<std::shared_ptr<MRequest>> getRequestHistory();

	void onLogin(const m::MegaError* e, OwningAccountPtr masp);

    Favourites favourites;
    void loadFavourites(OwningAccountPtr macc);
    void saveFavourites(OwningAccountPtr macc);

    std::unique_ptr<m::MegaNode> findNode(m::MegaHandle h);


    static std::string ToBase64(const std::string& s);
    static std::string FromBase64(const std::string& s);

    Users users;

private:
	std::mutex m;
	std::vector<OwningAccountPtr> megaAccounts;
    std::vector<OwningFolderPtr> megaFolderLinks;
    std::deque<std::shared_ptr<MRequest>> requestHistory;
};

extern MEGA* g_mega;



