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
			if (fs::is_directory(d->path()) && d->path().filename().u8string().find("-failed") == string::npos)
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
                    masp->setLoggingName(d->path().filename().u8string().c_str());

                    auto mcsp = make_shared<c::MegaChatApi>(masp.get());
                    mcsp->init(nullptr);

                    megaAccounts.push_back(make_shared<Account>(d->path(), masp, mcsp));
                    masp->fastLogin(sid.c_str(), new OneTimeListener([this, account = megaAccounts.back()](m::MegaRequest* request, m::MegaError* e) { onLogin(e, account); }));
                }
                else if (!link.empty() && LocalUserCrypt(link, false))
                {
                    auto masp = make_shared<m::MegaApi>("BTgWXaYb", d->path().u8string().c_str(), "Files++");
                    masp->setLoggingName(d->path().filename().u8string().c_str());
                    megaFolderLinks.push_back(make_shared<PublicFolder>(link, d->path(), masp));
                    masp->loginToFolder(link.c_str(), new OneTimeListener([this, folderlink = megaFolderLinks.back()](m::MegaRequest* request, m::MegaError* e) { onLogin(e, folderlink); }));
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
    std::vector<std::weak_ptr<Account>> aps;
    std::vector<std::weak_ptr<PublicFolder>> fps;
    std::vector<std::weak_ptr<m::MegaApi>> mas;
    
    {
        std::lock_guard g(m);
        for (auto& a : megaAccounts) { aps.push_back(a); mas.push_back(a->masp); a.reset(); }
        for (auto& a : megaFolderLinks) { fps.push_back(a); mas.push_back(a->masp); a.reset();  }
    }

    do
    {
        while (aps.size() && aps.back().expired()) aps.pop_back();
        while (fps.size() && fps.back().expired()) fps.pop_back();
        while (mas.size() && mas.back().expired()) mas.pop_back();
        WaitMillisec(10);
    } while (aps.size() || fps.size() || mas.size());

}

std::unique_ptr<m::MegaNode> MEGA::findNode(m::MegaHandle h)
{
    std::lock_guard g(m);
    for (auto& a : megaAccounts) 
    { 
        auto p = a->masp->getNodeByHandle(h);
        if (p) return std::unique_ptr<m::MegaNode>(p);
    }
    for (auto& a : megaFolderLinks) 
    { 
        auto p = a->masp->getNodeByHandle(h);
        if (p) return std::unique_ptr<m::MegaNode>(p);
    }
    return nullptr;
}

void MEGA::loadFavourites(OwningAccountPtr macc)
{
    ifstream faves((macc ? macc->cacheFolder : BasePath()) / "favourites");
    string s;
    while (std::getline(faves, s))
    {
        while (!s.empty() && isspace(s.back())) s.pop_back();
        if (s.empty()) continue;
        auto mp = MetaPath::deserialize(s);
        if (mp) favourites.Toggle(mp);
    }
}

void MEGA::saveFavourites(OwningAccountPtr macc)
{
    ofstream faves((macc ? macc->cacheFolder : BasePath()) / "favourites");
    for (auto& f : favourites.copy()) { string s; if (f.Account() == macc && f.serialize(s)) faves << s << std::endl; }
}

auto MEGA::findMegaApi(uint64_t dragdroptoken) -> OwningApiPtr
{
    std::lock_guard g(m);
    for (auto& a : megaAccounts) { if (reinterpret_cast<uint64_t>(a->masp.get()) == dragdroptoken) return a->masp; }
    for (auto& a : megaFolderLinks) { if (reinterpret_cast<uint64_t>(a->masp.get()) == dragdroptoken) return a->masp; }
    return {};
}

void MEGA::AddMRequest(ApiPtr masp, shared_ptr<MRequest>&& mrsp)
{
    std::lock_guard g(m);
    //for (auto& a : megaAccounts) { if (a->masp == masp) a->; }
    //for (auto& a : megaFolderLinks) { if (a->masp == masp) a->; }
    requestHistory.push_back(move(mrsp));
}

std::deque<std::shared_ptr<MRequest>> MEGA::getRequestHistory()
{
    std::lock_guard g(m);
    return requestHistory;
}

void MEGA::onLogin(const m::MegaError* e, OwningAccountPtr macc)
{
	if (e && e->getErrorCode())
	{
		ReportError("MEGA Login Failed: ", e);
	}
	else
	{
		macc->masp->fetchNodes(new OneTimeListener([this, macc](m::MegaRequest* request, m::MegaError* e)
			{
				if (e && e->getErrorCode()) ReportError("MEGA account FetchNodes failed: ", e);
                ++accountsUpdateCount;
                ++folderLinksUpdateCount;
                updateCV.notify_all();
                loadFavourites(macc);
                if (macc->mcsp)
                {
                    macc->mcsp->connect(new OneTimeChatListener([wcp = WeakAccountPtr(macc)](c::MegaChatRequest* request, c::MegaChatError* ce) {
                        if (auto macc = wcp.lock())
                        {
                            macc->chatState = ce && ce->getErrorCode() ? "<chat connect failed>" : macc->chatState = "<chat>";
                        }
                    }));
                }
			}));
	}
    ++accountsUpdateCount;
    ++folderLinksUpdateCount;
    updateCV.notify_all();
}

OwningAccountPtr MEGA::makeTempAccount()
{
    string foldername = to_string(time(NULL));
    fs::path p = BasePath() / foldername;
    fs::create_directories(p);
    auto megaapi = make_shared<m::MegaApi>("BTgWXaYb", p.u8string().c_str(), "Files++");
    auto megachatapi = make_shared<c::MegaChatApi>(megaapi.get());
    megachatapi->init(nullptr);
    auto ptr = make_shared<Account>(p, megaapi, megachatapi);
    ptr->masp->setLoggingName(foldername.c_str());
    return ptr;
}

OwningFolderPtr MEGA::makeTempFolder()
{
    string foldername = to_string(time(NULL));
    fs::path p = BasePath() / foldername;
    fs::create_directories(p);
    auto ptr = make_shared<PublicFolder>("", p, make_shared<m::MegaApi>("BTgWXaYb", p.u8string().c_str(), "Files++"));
    ptr->masp->setLoggingName(foldername.c_str());
    return ptr;
}

void MEGA::addAccount(OwningAccountPtr& a) {
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

void MEGA::addFolder(OwningFolderPtr& a) {
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

void MEGA::logoutremove(OwningAccountPtr ap)
{
	if (ap->masp->isLoggedIn())
	{
		ap->masp->logout(new OneTimeListener([this, ap](m::MegaRequest*, m::MegaError* e) {
			if (e && e->getErrorCode() != m::MegaError::API_OK) ReportError("Logout API error: ", e);
            ap->mcsp->logout(new OneTimeChatListener([this, ap](c::MegaChatRequest* request, c::MegaChatError* ce) {
                if (ce && ce->getErrorCode() != c::MegaChatError::ERROR_OK) ReportError("Logout Chat error: ", ce);
                deletecache(ap);
                }));
			}));
	}
	else
	{
		deletecache(ap);
	}
}

void MEGA::deletecache(OwningAccountPtr ap)
{
	std::lock_guard g(m);
    for (auto it = megaAccounts.begin(); it != megaAccounts.end(); ++it)
    {
        if (*it == ap)
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
        if ((*it)->masp == ap->masp)
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

std::string MEGA::ToBase64(const std::string& s) 
{ 
    return OwnString(m::MegaApi::binaryToBase64(s.data(), s.size())); 
}

std::string MEGA::FromBase64(const std::string& s)
{
    unsigned char* binary;
    size_t binarysize;
    m::MegaApi::base64ToBinary(s.c_str(), &binary, &binarysize);
    std::string ret((const char*)binary, binarysize);
    delete[] binary;
    return ret;
}

MRequest::MRequest(const OwningApiPtr& masp, const std::string& a)
    : OneTimeListener(nullptr)
    , mawp(masp)
    , action(a)
{
    g_mega->AddMRequest(masp, std::shared_ptr<MRequest>(this));
}

MRequest::MRequest(const OwningApiPtr& masp, const std::string& a, std::function<void(m::MegaRequest* request, m::MegaError* e)> f) 
    : OneTimeListener(f) 
    , mawp(masp)
    , action(a)
{
    g_mega->AddMRequest(masp, std::shared_ptr<MRequest>(this));
}



