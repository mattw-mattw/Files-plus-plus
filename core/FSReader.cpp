// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "FSReader.h"
#include "PlatformSupplied.h"
#include "MEGA.h"
#include "Item.h"
#include <fstream>
#include <mega/json.h>

using namespace std;


void UserFeedback::SetUserFeedback(const std::ostringstream& s)
{
    SetUserFeedbackStr(s.str());
}

void UserFeedback::SetUserFeedbackStr(const std::string& s)
{
    SetUserFeedbackCStr(s.c_str());
}


void FSReader::Send()
{
    if (!currentitems.empty())
    {
        q.push(Entry{ currentaction, move(currentitems) });
        currentitems.clear();
        if (trigger) trigger();
    }
    currentaction = INVALID_ACTION;
}

void FSReader::Queue(Action action, unique_ptr<Item>&& p)
{
    if (currentitems.size() >= 100 || (currentaction != action && !(action == RENAMEDFROM && currentaction == RENAMEDFROM)))
    {
        Send();
    }
    currentaction = action;
    currentitems.emplace_back(move(p));
}

void FSReader::OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths)
{
    ReportError("This view can't accept files, sorry");
}

void FSReader::OnDragDroppedMEGAItems(OwningApiPtr masp, const deque<unique_ptr<m::MegaNode>>& nodes)
{
    ReportError("This view can't accept files, sorry");
}

TopShelfReader::TopShelfReader(QueueTrigger t, bool r, UserFeedback& uf)
    : FSReader(t, r, uf)
    , workerthread([this]() { Threaded(); })
{
}

TopShelfReader::~TopShelfReader()
{
    cancelling = true;
    g_mega->updateCV.notify_all();
    workerthread.join();
}

void TopShelfReader::Threaded()
{
    for (;;)
    {
        int fluc = g_mega->accountsUpdateCount;

        Queue(NEWITEM, std::make_unique<Item>("<Local Volumes>"));
        auto megaAccounts = g_mega->accounts();
        for (auto& p : megaAccounts)
        {
            OwnString email(p->masp->getMyEmail());
            Queue(NEWITEM, std::make_unique<ItemMegaAccount>(email.empty() ? string("<loading>") : email, p));
        }
        auto megaFolders = g_mega->folderLinks();
        for (auto& p : megaFolders)
        {
            Queue(NEWITEM, std::make_unique<ItemMegaAccount>(p->folderLink, p));
        }
        Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
        Queue(NEWITEM, std::make_unique<Item>("<MEGA Command History>"));
        Send();

        unique_lock<mutex> g(g_mega->updateCVMutex);
        g_mega->updateCV.wait(g, [&]() { return cancelling || fluc != g_mega->accountsUpdateCount;  });

        if (!cancelling)
        {
            Queue(FILE_ACTION_APP_READRESTARTED, NULL);
            Send();
        }
        else break;
    }
}

bool checkProAccount()
{
    atomic<int> pro = 0, nonpro = 0;
    auto accounts = g_mega->accounts();
    for (auto ptr : accounts)
    {
        ptr->masp->getSpecificAccountDetails(false, false, true, 101, new OneTimeListener([&](m::MegaRequest* request, m::MegaError* e) {
            if ((!e || e->getErrorCode() == m::MegaError::API_OK) && request->getMegaAccountDetails()->getProLevel() > m::MegaAccountDetails::ACCOUNT_TYPE_FREE) ++pro;
            else ++nonpro;
            g_mega->updateCV.notify_all();
        }));
    }

    unique_lock g(g_mega->updateCVMutex);
    g_mega->updateCV.wait(g, [&]() { return pro + nonpro == accounts.size(); });

    return pro >= 1 || nonpro == 0;
}

auto TopShelfReader::GetMenuActions(shared_ptr<deque<Item*>> selectedItems) -> MenuActions
{
    MenuActions ma;
    if (selectedItems->size() == 1)
    {
        if (auto account = dynamic_cast<ItemMegaAccount*>((*selectedItems)[0]))
        {
            ma.actions.emplace_back("Log Out and remove local cache", [wap = account->wap]() { if (auto ap = wap.lock()) g_mega->logoutremove(ap); });
        }
    }
    else if (selectedItems->empty())
    {
        ma.actions.emplace_back("Add MEGA Account", []() { if (checkProAccount()) AddMEGAAccount(); else ReportError("Please load a PRO/Business account first"); });
        ma.actions.emplace_back("Add MEGA Folder Link", []() { AddMEGAFolderLink(); });
    }
    return ma;
};

MegaAccountReader::MegaAccountReader(WeakAccountPtr p, QueueTrigger t, bool r, UserFeedback& uf)
    : FSReader(t, r, uf)
    , wap(p)
    , workerthread([this]() { Threaded(); })
{
}

MegaAccountReader::~MegaAccountReader()
{
    cancelling = true;
    workerthread.join();
}

auto MegaAccountReader::GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) -> MenuActions
{
    return MenuActions();
}

void MegaAccountReader::Threaded()
{
    // todo: make sure readers are destroyed so shared ptr is released

    auto acc = wap.lock();
    if (!acc) return;
    auto& masp = acc->masp;

    unique_ptr<m::MegaNode> root(masp->getRootNode());
    unique_ptr<m::MegaNode> inbox(masp->getInboxNode());
    unique_ptr<m::MegaNode> rubbish(masp->getRubbishNode());

    Queue(NEWITEM, std::make_unique<ItemMegaChatRoot>(acc->chatState, acc));

    if (root) Queue(NEWITEM, std::make_unique<ItemMegaNode>(root->getName(), move(root)));
    if (inbox) Queue(NEWITEM, std::make_unique<ItemMegaNode>(inbox->getName(), move(inbox)));
    if (rubbish) Queue(NEWITEM, std::make_unique<ItemMegaNode>(rubbish->getName(), move(rubbish)));

    unique_ptr<m::MegaNodeList> inshares(masp->getInShares());
    if (inshares)
    {
        for (int i = 0; i < inshares->size(); ++i)
        {
            Queue(NEWITEM, std::make_unique<ItemMegaInshare>(std::unique_ptr<m::MegaNode>(inshares->get(i)->copy()), *masp, -1, true));
        }
    }

    Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
    Send();
}

MegaFSReader::MegaFSReader(OwningAccountPtr p, std::unique_ptr<m::MegaNode> n, QueueTrigger t, bool r, UserFeedback& uf)
    : FSReader(t, r, uf)
    , masp(p->masp)
    , mnode(move(n))
    , workerthread([this]() { Threaded(); })
{
}

MegaFSReader::~MegaFSReader()
{
    cancelling = true;
    listener.nq.push(nullptr);
    workerthread.join();
}

auto MegaFSReader::GetMenuActions(shared_ptr<deque<Item*>> selectedItems) -> MenuActions
{
    MenuActions ma;
    if (selectedItems->size())
    {

        if (selectedItems->size() == 1)
        {
            if (auto n = dynamic_cast<ItemMegaNode*>(selectedItems->front()))
            {
                if (n->mnode->isExported())
                {
                    ma.actions.emplace_back("Copy Public Link (with key)", [=, masp = masp]()
                    {
                        PutStringToClipboard(OwnString(n->mnode->getPublicLink()));
                    });
                }
                else
                {
                    ma.actions.emplace_back("Export Link", [=, masp = masp]()
                    {
                        masp->exportNode(n->mnode.get(), new MRequest(masp, "Export Link", [&](m::MegaRequest* request, m::MegaError* e) {
                            if (e && e->getErrorCode() == m::MegaError::API_OK) PutStringToClipboard(request->getLink());
                        }));
                    });
                }
            }
        }

        ma.actions.emplace_back("Send to Trash", [=, masp = masp]()
            {  
                unique_ptr<m::MegaNode> bin(masp->getRubbishNode()); 
                for (auto i : *selectedItems) 
                    if (auto n = dynamic_cast<ItemMegaNode*>(i)) 
                        masp->moveNode(n->mnode.get(), bin.get()); 
            });

        ma.actions.emplace_back("Delete (no undo)", [=, masp = masp]()
            { 
            if (QueryUserOkCancel("Please confirm permanent delete"))
                for (auto i : *selectedItems)
                    if (auto n = dynamic_cast<ItemMegaNode*>(i))
                        masp->remove(n->mnode.get(), new MRequest(masp, "Delete node " + OwnString(masp->getNodePath(n->mnode.get()))));
            });
    }
    return ma;
};

void MegaFSReader::OnDragDroppedMEGAItems(OwningApiPtr source_masp, const deque<unique_ptr<m::MegaNode>>& nodes)
{
    OwnString targetPath(masp->getNodePath(mnode.get()));
    targetPath += "/";
    for (auto& n : nodes)
    {
        OwnString s(masp->getNodePath(n.get()));
        s += "/";
        if (s.size() <= targetPath.size() && memcmp(s.data(), targetPath.data(), s.size()) == 0)
        {
            ReportError("Copying " + OwnString(masp->getNodePath(n.get())) + " to " + targetPath + " would be problematic");
            return;
        }
        else if (s.size() > targetPath.size() && memcmp(s.data(), targetPath.data(), targetPath.size()) == 0)
        {
            if (s.find("/", targetPath.size()) == s.size()-1)
            {
                ReportError("This item is already in this folder");
                return;
            }
        }
    }

    if (source_masp == masp)
    {
        for (auto& n : nodes)
        {
            masp->copyNode(n.get(), mnode.get());
        }
        userFeedback.SetUserFeedback(ostringstream() << "Sent " << nodes.size() << " node copies");
    }
    else
    {
        ReportError("Sorry, cannot drag-drop between mega accounts.");
    }
}

void MegaFSReader::OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths)
{
    for (auto p : paths)
    {
        masp->startUpload(p.u8string().c_str(), mnode.get());
    }
    ReportError("Queued " + to_string(paths.size()) + " paths for upload");
}

string removebase(OwnStr& path, const string& basepath)
{
    if (strstr(path.get(), basepath.c_str()) == path.get() && path[basepath.size()] == '/')
    {
        return path.get() + basepath.size() + 1;
    }
    else
    {
        return path.get();
    }
}

void MegaFSReader::RecursiveRead(m::MegaNode& mnode, const string& basepath)
{
    unique_ptr<m::MegaChildrenLists> mc(masp->getFileFolderChildren(&mnode));
    if (mc)
    {
        m::MegaNodeList* folders = mc->getFolderList();
        m::MegaNodeList* files = mc->getFileList();

        for (int i = 0; i < folders->size(); ++i)
        {
            auto node = folders->get(i);
            nodes_present.insert(node->getHandle());
            OwnStr path(masp->getNodePath(node), true);
            Queue(NEWITEM, std::make_unique<ItemMegaNode>(removebase(path, basepath), std::unique_ptr<m::MegaNode>(node->copy())));
            if (recurse) RecursiveRead(*node, basepath);
        }
        for (int i = 0; i < files->size(); ++i)
        {
            auto node = files->get(i);
            nodes_present.insert(node->getHandle());
            OwnStr path(masp->getNodePath(node), true);
            Queue(NEWITEM, std::make_unique<ItemMegaNode>(removebase(path, basepath), std::unique_ptr<m::MegaNode>(node->copy())));
        }
    }
}

bool hasAncestor(std::unique_ptr<m::MegaNode> n, m::MegaNode* ancestor, m::MegaApi* api)
{
    for (; n != nullptr; n.reset(api->getParentNode(n.get())))
    {
        if (n->getHandle() == ancestor->getHandle()) return true;
    }
    return false;
}

void MegaFSReader::Threaded()
{
    masp->addGlobalListener(&listener);

    for (;;)
    {
        nodes_present.clear();
        bool reload_needed = false;
        OwnStr basepathptr(masp->getNodePath(mnode.get()), true);
        string basepath(basepathptr.get());
        if (basepath == "/") basepath = "";
        RecursiveRead(*mnode, basepath);
        Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
        Send();

        while (!cancelling && !reload_needed)
        {
            unique_ptr<m::MegaNodeList> nodes;
            if (listener.nq.pop(nodes, true))
            {
                if (cancelling) break;
                if (!nodes)
                {
                    // MEGA callback says to reload, due to too many changes
                    Queue(FILE_ACTION_APP_READRESTARTED, NULL);
                    reload_needed = true;
                    break;
                }
                for (int i = 0; i < nodes->size(); ++i)
                {
                    auto n = nodes->get(i);
                    if (n->isFolder() || n->isFile())
                    {
                        if (recurse && hasAncestor(unique_ptr<m::MegaNode>(n->copy()), mnode.get(), masp.get()) ||
                            !recurse && n->getParentHandle() == mnode->getHandle())
                        {
                            if (n->hasChanged(m::MegaNode::CHANGE_TYPE_REMOVED))
                            {
                                if (nodes_present.find(n->getHandle()) != nodes_present.end())
                                {
                                    Queue(DELETEDITEM, std::make_unique<ItemMegaNode>(n->getName(), std::unique_ptr<m::MegaNode>(n->copy())));
                                    nodes_present.erase(n->getHandle());
                                }
                            }
                            else if (n->hasChanged(m::MegaNode::CHANGE_TYPE_NEW))
                            {
                                OwnStr path(masp->getNodePath(n), true);
                                string name(removebase(path, basepath));
                                nodes_present.insert(n->getHandle());
                                Queue(NEWITEM, std::make_unique<ItemMegaNode>(name, std::unique_ptr<m::MegaNode>(n->copy())));

                            }
                            else if (n->hasChanged(m::MegaNode::CHANGE_TYPE_ATTRIBUTES)) // could be renamed
                            {
                                OwnStr path(masp->getNodePath(n), true);
                                string name(removebase(path, basepath));
                                Queue(DELETEDITEM, std::make_unique<ItemMegaNode>(name, std::unique_ptr<m::MegaNode>(n->copy())));
                                Queue(NEWITEM, std::make_unique<ItemMegaNode>(name, std::unique_ptr<m::MegaNode>(n->copy())));
                            }
                        }
                        else if (n->hasChanged(m::MegaNode::CHANGE_TYPE_PARENT))
                        {
                            if (nodes_present.find(n->getHandle()) != nodes_present.end())
                            {
                                OwnStr path(masp->getNodePath(n), true);
                                string name(removebase(path, basepath));
                                Queue(DELETEDITEM, std::make_unique<ItemMegaNode>(name, std::unique_ptr<m::MegaNode>(n->copy())));
                                nodes_present.erase(n->getHandle());
                            }
                        }
                    }
                }
                Send();
            }
        }
        if (!reload_needed) break;
    }
    masp->removeGlobalListener(&listener);
}


MegaChatRoomsReader::MegaChatRoomsReader(OwningAccountPtr p, QueueTrigger t, bool r, UserFeedback& uf)
    : FSReader(t, r, uf)
    , ap(p)
    , workerthread([this]() { Threaded(); })
{
}

MegaChatRoomsReader::~MegaChatRoomsReader()
{
    cancelling = true;
    listener.nq.push(nullptr);
    workerthread.join();
}

auto MegaChatRoomsReader::GetMenuActions(shared_ptr<deque<Item*>> selectedItems) -> MenuActions
{
    MenuActions ma;
    if (selectedItems->size())
    {

        if (selectedItems->size() == 1)
        {
            if (auto n = dynamic_cast<ItemMegaNode*>(selectedItems->front()))
            {
                //ma.actions.emplace_back("Export Link", [=, masp = masp]()
                //{
                //    masp->exportNode(n->mnode.get(), new MRequest(masp, "Export Link", [&](m::MegaRequest* request, m::MegaError* e) {
                //        if (e && e->getErrorCode() == m::MegaError::API_OK) PutStringToClipboard(request->getLink());
                //    }));
                //});
            }
        }

    }
    return ma;
};

void MegaChatRoomsReader::OnDragDroppedMEGAItems(OwningApiPtr source_masp, const deque<unique_ptr<m::MegaNode>>& nodes)
{
    ReportError("Sorry, drag-drop not supported here yet.");
}

void MegaChatRoomsReader::OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths)
{
    ReportError("Sorry, drag-drop not supported here yet.");
}

void MegaChatRoomsReader::Threaded()
{
    ap->masp->addGlobalListener(&listener);

    std::unique_ptr<c::MegaChatRoomList> crl(ap->mcsp->getChatRooms());
    if (crl)
    {
        for (unsigned i = 0; i < crl->size(); ++i)
        {
            std::unique_ptr<c::MegaChatRoom> up(crl->get(i)->copy());
            string name = OwnString(up->getTitle());
            Queue(NEWITEM, std::make_unique<ItemMegaChat>(name, move(up)));
        }
    }
    Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
    Send();

    ap->masp->removeGlobalListener(&listener);
}


MegaChatReader::MegaChatReader(OwningAccountPtr p, QueueTrigger t, bool r, UserFeedback& uf)
    : FSReader(t, r, uf)
    , ap(p)
    , workerthread([this]() { Threaded(); })
{
}

MegaChatReader::~MegaChatReader()
{
    cancelling = true;
    listener.nq.push(nullptr);
    workerthread.join();
}

auto MegaChatReader::GetMenuActions(shared_ptr<deque<Item*>> selectedItems) -> MenuActions
{
    MenuActions ma;
    if (selectedItems->size())
    {

        if (selectedItems->size() == 1)
        {
            if (auto n = dynamic_cast<ItemMegaNode*>(selectedItems->front()))
            {
                //ma.actions.emplace_back("Export Link", [=, masp = masp]()
                //{
                //    masp->exportNode(n->mnode.get(), new MRequest(masp, "Export Link", [&](m::MegaRequest* request, m::MegaError* e) {
                //        if (e && e->getErrorCode() == m::MegaError::API_OK) PutStringToClipboard(request->getLink());
                //    }));
                //});
            }
        }

    }
    return ma;
};

void MegaChatReader::OnDragDroppedMEGAItems(OwningApiPtr source_masp, const deque<unique_ptr<m::MegaNode>>& nodes)
{
    ReportError("Sorry, drag-drop not supported here yet.");
}

void MegaChatReader::OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths)
{
    ReportError("Sorry, drag-drop not supported here yet.");
}

void MegaChatReader::Threaded()
{
    ap->masp->addGlobalListener(&listener);

    //    for (;;)
    {
        //      Queue(NEWITEM, std::make_unique<ItemMegaNode>(name, std::unique_ptr<m::MegaNode>(n->copy())));
    }
    Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
    Send();

    ap->masp->removeGlobalListener(&listener);
}


CommandHistoryReader::CommandHistoryReader(QueueTrigger t, UserFeedback& uf)
    : FSReader(t, false, uf)
    , workerthread([this]() { Threaded(); })
{
}

CommandHistoryReader::~CommandHistoryReader()
{
    cancelling = true;
    workerthread.join();
}

auto CommandHistoryReader::GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) -> MenuActions
{
    return MenuActions();
}

void CommandHistoryReader::Threaded()
{
    auto history = g_mega->getRequestHistory();
    for (auto& c : history)
    {
        Queue(NEWITEM, std::make_unique<ItemCommand>(c));
    }

    Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
    Send();
}

PlaylistReader::PlaylistReader(const std::filesystem::path& path, QueueTrigger t, bool r, UserFeedback& uf)
    : FSReader(t, r, uf)
    , fullFilePath(path)
    , workerthread([this]() { Threaded(); })
{
}

PlaylistReader::~PlaylistReader()
{
    cancelling = true;
    workerthread.join();
}

auto PlaylistReader::GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) -> MenuActions
{
    MenuActions ma;
    ma.actions.emplace_back("Remove Song(s)", [this, selectedItems]()
    {
        for (Item* item : *selectedItems)
        {
            if (auto p = dynamic_cast<ItemMegaNode*>(item))
            {
                Queue(DELETEDITEM, std::make_unique<ItemMegaNode>(string(p->mnode->getName()), std::unique_ptr<m::MegaNode>(p->mnode->copy())));
            }
        }
        Send();
    });
    return ma;
}

void PlaylistReader::OnDragDroppedMEGAItems(OwningApiPtr source_masp, const deque<unique_ptr<m::MegaNode>>& nodes)
{
    for (auto& n : nodes)
    {
        Queue(NEWITEM, std::make_unique<ItemMegaNode>(string(n->getName()), std::unique_ptr<m::MegaNode>(n->copy())));
    }
    Send();
}

void PlaylistReader::Threaded()
{
    std::ifstream file(fullFilePath, ios::binary);    // bin becuase cross-platform

    std::string content;
    while (file && !file.eof())
    {
        auto c = file.get();
        if (c != std::ifstream::traits_type::eof())
        {
            content.append(1, char(c));
        }
    }

    if (!file && !file.eof())
    {
        Queue(FILE_ACTION_APP_ERROR, NULL);
        return;
    }

    ::mega::JSON json;
    json.pos = content.c_str();
    if (json.enterarray())
    {
        for (;;)
        {
            if (json.enterobject())
            {
                ::mega::handle h = ::mega::UNDEF;
                for (;;)
                {
                    switch (json.getnameid())
                    {
                    case 'h':
                        h = json.gethandle();
                        break;
                    case EOO:
                        if (auto p = g_mega->findNode(h))
                        {
                            Queue(NEWITEM, std::make_unique<ItemMegaNode>(string(p->getName()), std::move(p)));
                        }
                        goto breakInnerFor;
                    default:
                        json.storeobject();
                    }

                }
                breakInnerFor: if (!json.leaveobject()) break;
            }
            else break;
        }
        json.leavearray();
    }

    Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
    Send();
}
