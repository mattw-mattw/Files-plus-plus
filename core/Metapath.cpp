// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "MetaPath.h"
#include "MEGA.h"
#include "PlatformSupplied.h"

using namespace std;

bool MetaPath::Ascend()
{
    switch (pathType)
    {
        case TopShelf: {
            return false;
        }
        case LocalFS: {
            auto newPath = localPath.parent_path();
            if (newPath == localPath)
            {
                localPath.clear();
                pathType = LocalVolumes;
            }
            else
            {
                localPath = newPath;
            }
            return true;
        }
        case Playlist: {
            localPath = localPath.parent_path();
            pathType = LocalFS;
            return true;
        }
        case LocalVolumes: {
            pathType = TopShelf;
            return true;
        }
        case MegaAccount: {
            pathType = TopShelf;
            return true;
        }
        case MegaChats: {
            pathType = MegaAccount;
            return true;
        }
        case MegaChat: {
            pathType = MegaChats;
            return true;
        }
        case MegaFS: {
            if (auto p = wap.lock())
            {
                unique_ptr<m::MegaNode> n(p->masp->getParentNode(mnode.get()));
                if (n)
                {
                    mnode.reset(n.release());
                }
                else
                {
                    pathType = MegaAccount;
                    wap = g_mega->getAccount(p->masp);
                }
                return true;
            }
        }
        case CommandHistory:{
            pathType = TopShelf;
            return true;
        }
    }
    return false;
}

bool MetaPath::Descend(const Item& item)
{
    switch (pathType)
    {
    case TopShelf: {
        if (auto i = dynamic_cast<const ItemMegaAccount*>(&item))
        {
            if (auto ap = i->wap.lock())
            {
                pathType = MegaAccount;
                wap = i->wap;
                mnode.reset();
            }
            else break;
        }
        else if (item.u8Name == "<MEGA Command History>")
        {
            pathType = CommandHistory;
            wap.reset();
            mnode.reset();
        }
        else
        {
            pathType = LocalVolumes;
            mnode.reset();
            wap.reset();
        }
        return true;
    }
    case LocalFS: {
        fs::path newpath = localPath / fs::u8path(item.u8Name);
        std::error_code ec;
        if (fs::is_directory(newpath, ec))
        {
            localPath = newpath;
            return true;
        }
        else if (ec)
        {
            ReportError("Could not open directory: " + ec.message());
            return false;
        }
        if (auto p = dynamic_cast<const ItemLocalFS*>(&item))
        {
            if (p->u8Name.size() > 9 && p->u8Name.substr(p->u8Name.size() - 9) == ".playlist")
            {
                pathType = Playlist;
                localPath = newpath;
                return true;
            }
        }
        break;
    }
    case Playlist: {
        break;
    }
    case LocalVolumes: {
        auto p = fs::u8path(item.u8Name + "\\");
        if (fs::is_directory(p))
        {
            localPath = p;
            pathType = LocalFS;
            return true;
        }
        break;
    }
    case MegaAccount: {
        if (auto i = dynamic_cast<const ItemMegaNode*>(&item))
        {
            mnode.reset(i->mnode->copy());
            pathType = MegaFS;
            return true;
        }
        else if (auto i = dynamic_cast<const ItemMegaInshare*>(&item))
        {
            mnode.reset(i->mnode->copy());
            pathType = MegaFS;
            return true;
        }
        else if (auto i = dynamic_cast<const ItemMegaChatRoot*>(&item))
        {
            //mnode.reset(i->mnode->copy());
            pathType = MegaChats;
            return true;
        }
    }
    case MegaChats: {
        if (auto p = dynamic_cast<const ItemMegaChat*>(&item))
        {
            pathType = MegaChat;
            chatroom.reset(p->room->copy());
            return true;
        }
        return false;
    }
    case MegaFS: {
        if (auto i = dynamic_cast<const ItemMegaNode*>(&item))
        if (i->isFolder())
        {
            mnode.reset(i->mnode->copy());
            return true;
        }
    }
    }
    return false;
}

void MetaPath::SetLocalPath(const fs::path& p)
{
    pathType = LocalFS;
    localPath = p;
}

bool MetaPath::GetLocalPath(std::filesystem::path& p) const
{
    if (pathType == LocalFS || pathType == Playlist)
    {
        p = localPath;
        return true;
    }
    return false;
}

bool MetaPath::GetDragDropUNCPath(Item* pItem, std::string& uncPath)
{
    switch (pathType)
    {
    case LocalFS:   uncPath = PlatformLocalUNCPrefix() + (localPath / pItem->u8Name).u8string();
                    return true;
    case MegaFS:    if (auto p = dynamic_cast<ItemMegaNode*>(pItem)) 
                        if (auto ap = wap.lock())
                        {   
                            uncPath = PlatformMegaUNCPrefix(ap->masp.get()) + ap->masp->getNodePath(p->mnode.get());
                            return true;
                        }
    default:        break;
    }
    return false;
}


string MetaPath::GetFullPath(Item& item)
{
    string s;
    switch (pathType)
    {
    case LocalFS: s = (localPath / fs::u8path(item.u8Name)).u8string();
        break;

    case MegaFS: if (auto p = dynamic_cast<ItemMegaNode*>(&item))
                    if (auto ap = wap.lock())
                        s = ap->masp->getNodePath(p->mnode.get());  
                break;
    }
    return s;
}

unique_ptr<FSReader> MetaPath::GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const
{
    switch (pathType)
    {
    case TopShelf: return make_unique<TopShelfReader>(t, recurse, uf);
    case LocalFS: return NewLocalFSReader(localPath, t, recurse, uf);
    case Playlist: return make_unique<PlaylistReader>(localPath, t, recurse, uf);
    case LocalVolumes: return make_unique<LocalVolumeReader>(t, recurse, uf);
    case MegaAccount: if (auto ap = wap.lock()) return make_unique<MegaAccountReader>(ap, t, recurse, uf);
    case MegaChats:  if (auto ap = wap.lock()) return make_unique<MegaChatRoomsReader>(ap, t, recurse, uf);
    case MegaChat: if (auto ap = wap.lock()) return make_unique<MegaChatReader>(ap, std::unique_ptr<c::MegaChatRoom>(chatroom->copy()), t, recurse, uf);
    case MegaFS: if (auto ap = wap.lock()) return make_unique<MegaFSReader>(ap, mnode.copy(), t, recurse, uf);
    case CommandHistory: return make_unique<CommandHistoryReader>(t, uf);
    default: assert(false);
    }
    return nullptr;
}

std::string MetaPath::u8DisplayPath() const
{
    switch (pathType)
    {
    case TopShelf: return "<Top Shelf>";
    case LocalFS: return localPath.u8string();
    case Playlist: return localPath.u8string();
    case LocalVolumes: return "<Local Volumes>";
    case MegaAccount: {
        if (auto ap = wap.lock())
        {
            OwnStr e(ap->masp->getMyEmail(), false);
            return string(e ? e.get() : "<null>");
        }
    }
    case MegaChats: return "<Chatrooms>";
    case MegaChat: return "<Chat>";
    case MegaFS: {
        if (auto ap = wap.lock())
        {
            return OwnString(ap->masp->getNodePath(mnode.get()));
        }
    }
    }
    return "";
}

bool MetaPath::operator==(const MetaPath& o) const
{
    if (pathType != o.pathType) return false;
    switch (pathType)
    {
    case TopShelf: return true;
    case LocalFS: return localPath == o.localPath;
    case Playlist: return localPath == o.localPath;
    case LocalVolumes: return true;
    case MegaAccount: return wap.lock() == o.wap.lock();
    case MegaChats: return wap.lock() == o.wap.lock();
    case MegaChat: return wap.lock() == o.wap.lock();
    case MegaFS: return wap.lock() == o.wap.lock() && mnode->getHandle() == o.mnode->getHandle();
    }
    return false;
}

std::function<bool(unique_ptr<Item>& a, unique_ptr<Item>& b)> MetaPath::nodeCompare()
{
    switch (pathType)
    {
    case MegaFS: return [](unique_ptr<Item>& a, unique_ptr<Item>& b)
    {
        return static_cast<ItemMegaNode*>(a.get())->mnode->getHandle() < static_cast<ItemMegaNode*>(b.get())->mnode->getHandle();
    };
    default: return [](unique_ptr<Item>& a, unique_ptr<Item>& b)
    {
        return a->u8Name < b->u8Name;
    };
    }
}

OwningAccountPtr MetaPath::Account()
{
    switch (pathType)
    {
    case TopShelf: 
    case LocalFS: 
    case Playlist:
    case LocalVolumes:  return {};
    case MegaAccount:
    case MegaChats:
    case MegaChat:
    case MegaFS: return wap.lock();
    }
    return {};
}

bool MetaPath::serialize(std::string& s)
{
    switch (pathType)
    {
        case TopShelf: {
            s = "TopShelf";
            return true;
        }
        case LocalFS: {
            s = "LocalFS/" + MEGA::ToBase64(localPath.u8string());
            return true;
        }
        case Playlist: {
            s = "Playlist/" + MEGA::ToBase64(localPath.u8string());
            return true;
        }
        case LocalVolumes: {
            s = "LocalVolumes";
            return true;
        }
        case MegaAccount: {
            if (auto ap = wap.lock())
            {
                s = "MegaAccount/" + MEGA::ToBase64(OwnString(ap->masp->getMyEmail()));
                return true;
            }
        }
        case MegaFS: {
            if (auto ap = wap.lock())
            {
                s = "MegaFS/" + MEGA::ToBase64(OwnString(ap->masp->getMyEmail())) + "/" + MEGA::ToBase64(ap->masp->getNodePath(mnode.get()));
                return true;
            }
        }
        case MegaChats: // todo
        case MegaChat:;
    }
    assert(false);    
    return false;
}

MetaPath MetaPath::deserialize(const std::string& s)
{
    MetaPath mp;
    if (s == "TopShelf")
    {
        mp.pathType = TopShelf;
        return mp;
    }
    if (s.size() >= 9 && s.substr(0, 8) == "LocalFS/")
    {
        mp.pathType = LocalFS;
        mp.localPath = MEGA::FromBase64(s.substr(8));
        return mp;
    }
    if (s.size() >= 9 && s.substr(0, 9) == "Playlist/")
    {
        mp.pathType = Playlist;
        mp.localPath = MEGA::FromBase64(s.substr(9));
        return mp;
    }
    if (s == "LocalVolumes")
    {
        mp.pathType = LocalVolumes;
        return mp;
    }
    if (s.size() >= 13 && s.substr(0, 12) == "MegaAccount/")
    {
        mp.pathType = MegaAccount;
        auto acc = MEGA::FromBase64(s.substr(12));
        for (auto ptr : g_mega->accounts()) { 
            OwnString email(ptr->masp->getMyEmail());
            if (email == acc) 
            { 
                mp.wap = ptr; 
                return mp; 
            } 
        };
    }
    if (s.size() >= 8 && s.substr(0, 7) == "MegaFS/")
    {
        mp.pathType = MegaFS;
        auto n = s.find("/", 7);
        if (n+1 && n > 7)
        {
            auto acc = MEGA::FromBase64(s.substr(7, n-7));
            auto path = MEGA::FromBase64(s.substr(n+1));
            for (auto ptr : g_mega->accounts()) { if (OwnString(ptr->masp->getMyEmail()) == acc) { mp.wap = ptr; } };
            if (auto ap = mp.wap.lock()) mp.mnode.reset(ap->masp->getNodeByPath(path.c_str()));
            if (mp.wap.lock() && mp.mnode) return mp;
        }
    }
    assert(false);
    mp.pathType = None;
    return mp;
}
