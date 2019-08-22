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
        case LocalVolumes: {
            pathType = TopShelf;
            return true;
        }
        case MegaAccount: {
            pathType = TopShelf;
            return true;
        }
        case MegaFS: {
            unique_ptr<m::MegaNode> n(masp->getParentNode(mnode.get()));
            if (n)
            {
                mnode.reset(n.release());
            }
            else
            {
                pathType = MegaAccount;
                mnode.reset();
            }
            return true;
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
            pathType = MegaAccount;
            masp = i->masp;
            mnode.reset();
        }
        else if (item.u8Name == "<MEGA Command History>")
        {
            pathType = CommandHistory;
            masp.reset();
            mnode.reset();
        }
        else
        {
            pathType = LocalVolumes;
            mnode.reset();
            masp.reset();
        }
        return true;
    }
    case LocalFS: {
        fs::path newpath = localPath / fs::u8path(item.u8Name);
        if (fs::is_directory(newpath))
        {
            localPath = newpath;
            return true;
        }
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
    }
    case MegaFS: {
        if (auto i = dynamic_cast<const ItemMegaNode*>(&item))
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
    if (pathType == LocalFS)
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
                    {   
                        uncPath = PlatformMegaUNCPrefix(masp.get()) + masp->getNodePath(p->mnode.get());
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
    case LocalFS: s = (localPath / fs::u8path(item.u8Name)).u8string(); break;
    case MegaFS: if (auto p = dynamic_cast<ItemMegaNode*>(&item)) s = masp->getNodePath(p->mnode.get());  break;
    }
    return s;
}

unique_ptr<FSReader> MetaPath::GetContentReader(FSReader::QueueTrigger t, bool recurse) const
{
    switch (pathType)
    {
    case TopShelf: return make_unique<TopShelfReader>(t, recurse);
    case LocalFS: return NewLocalFSReader(localPath, t, recurse);
    case LocalVolumes: return make_unique<LocalVolumeReader>(t, recurse);
    case MegaAccount: return make_unique<MegaAccountReader>(masp, t, recurse);
    case MegaFS: return make_unique<MegaFSReader>(masp, mnode.copy(), t, recurse);
    case CommandHistory: return make_unique<CommandHistoryReader>(t);
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
    case LocalVolumes: return "<Local Volumes>";
    case MegaAccount: {
        std::unique_ptr<char[]> e(masp->getMyEmail());
        return string(e ? e.get() : "<null>");
    }
    case MegaFS: {
        std::unique_ptr<char[]> p(masp->getNodePath(mnode.get()));
        return string(p.get());
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
    case LocalVolumes: return true;
    case MegaAccount: return masp == o.masp;
    case MegaFS: return masp == o.masp && mnode->getHandle() == o.mnode->getHandle();
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

ApiPtr MetaPath::Account()
{
    switch (pathType)
    {
    case TopShelf: 
    case LocalFS: 
    case LocalVolumes:  return nullptr;
    case MegaAccount:
    case MegaFS: return masp;
    }
    return nullptr;
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
        case LocalVolumes: {
            s = "LocalVolumes";
            return true;
        }
        case MegaAccount: {
            s = "MegaAccount/" + MEGA::ToBase64(OwnString(masp->getMyEmail()));
            return true;
        }
        case MegaFS: {
            s = "MegaFS/" + MEGA::ToBase64(OwnString(masp->getMyEmail())) + "/" + MEGA::ToBase64(masp->getNodePath(mnode.get()));
            return true;
        }
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
                mp.masp = ptr->masp; return mp; 
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
            for (auto ptr : g_mega->accounts()) { if (OwnString(ptr->masp->getMyEmail()) == acc) { mp.masp = ptr->masp; } };
            if (mp.masp) mp.mnode.reset(mp.masp->getNodeByPath(path.c_str()));
            if (mp.masp && mp.mnode) return mp;
        }
    }
    assert(false);
    mp.pathType = None;
    return mp;
}
