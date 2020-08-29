// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "MetaPath.h"
#include "MEGA.h"
#include "PlatformSupplied.h"

using namespace std;


std::unique_ptr<MetaPath> MetaPath_LocalFS::Ascend()
{
    auto newPath = localPath.parent_path();
    if (newPath == localPath)
    {
        localPath.clear();
        return std::make_unique<MetaPath_LocalVolumes>();
    }
    else
    {
        return std::make_unique<MetaPath_LocalFS>(newPath);
    }
}

std::unique_ptr<MetaPath> MetaPath_MegaFS::Ascend()
{
    if (auto p = wap.lock())
    {
        unique_ptr<m::MegaNode> n(p->masp->getParentNode(mnode.get()));
        if (n)
        {
            return std::make_unique<MetaPath_MegaFS>(p, move(n));
        }
        else
        {
            return std::make_unique<MetaPath_MegaAccount>(g_mega->getAccount(p->masp));
        }
    }
    return nullptr;
}


std::unique_ptr<MetaPath> MetaPath_TopShelf::Descend(const Item& item)
{
    if (auto i = dynamic_cast<const ItemMegaAccount*>(&item))
    {
        if (auto ap = i->wap.lock())
        {
            return std::make_unique<MetaPath_MegaAccount>(ap);
        }
    }
    //else if (item.u8Name == "<MEGA Command History>")
    //{
    //    if (auto ap = i->wap.lock())
    //    {
    //        return std::make_unique<MetaPath_CommandHistory>(ap);
    //    }
    //}
    else
    {
        return std::make_unique<MetaPath_LocalVolumes>();
    }
    return nullptr;
}

std::unique_ptr<MetaPath> MetaPath_LocalFS::Descend(const Item& item)
{
    fs::path newpath = localPath / fs::u8path(item.u8Name);
    std::error_code ec;
    if (fs::is_directory(newpath, ec))
    {
        return std::make_unique<MetaPath_LocalFS>(newpath);
    }
    else if (ec)
    {
        ReportError("Could not open directory: " + ec.message());
    }
    if (auto p = dynamic_cast<const ItemLocalFS*>(&item))
    {
        if (fs::is_regular_file(newpath, ec) && p->u8Name.size() > 9 && p->u8Name.substr(p->u8Name.size() - 9) == ".playlist")
        {
            return std::make_unique<MetaPath_Playlist>(newpath);
        }
    }
    return nullptr;
}

std::unique_ptr<MetaPath> MetaPath_LocalVolumes::Descend(const Item& item)
{
    auto p = fs::u8path(item.u8Name + "\\");
    std::error_code ec;
    if (fs::is_directory(p, ec))
    {
        return std::make_unique<MetaPath_LocalFS>(p);
    }
    return nullptr;
}

std::unique_ptr<MetaPath> MetaPath_MegaAccount::Descend(const Item& item)
{
    if (auto ap = wap.lock())
    {
        if (auto i = dynamic_cast<const ItemMegaNode*>(&item))
        {
            return std::make_unique<MetaPath_MegaFS>(ap, i->mnode.copy());
        }
        else if (auto i = dynamic_cast<const ItemMegaInshare*>(&item))
        {
            return std::make_unique<MetaPath_MegaFS>(ap, i->mnode.copy());
        }
        else if (auto i = dynamic_cast<const ItemMegaChatRoot*>(&item))
        {
            return std::make_unique<MetaPath_MegaChats>(ap);
        }
    }
    return nullptr;
}

std::unique_ptr<MetaPath> MetaPath_MegaChats::Descend(const Item& item)
{
    if (auto p = dynamic_cast<const ItemMegaChat*>(&item))
        if (auto ap = wap.lock())
            return std::make_unique<MetaPath_MegaChat>(ap, p->room.copy());
    return nullptr;
}

std::unique_ptr<MetaPath> MetaPath_MegaFS::Descend(const Item& item)
{
    if (auto i = dynamic_cast<const ItemMegaNode*>(&item))
        if (i->isFolder())
            if (auto ap = wap.lock())
                return std::make_unique<MetaPath_MegaFS>(ap, i->mnode.copy());
    return nullptr;
}



bool MetaPath_LocalFS::GetDragDropUNCPath(Item* pItem, std::string& uncPath)
{
    uncPath = PlatformLocalUNCPrefix() + (localPath / pItem->u8Name).u8string();
    return true;
}

bool MetaPath_MegaFS::GetDragDropUNCPath(Item* pItem, std::string& uncPath)
{
    if (auto p = dynamic_cast<ItemMegaNode*>(pItem))
        if (auto ap = wap.lock())
        {
            uncPath = PlatformMegaUNCPrefix(ap->masp.get()) + ap->masp->getNodePath(p->mnode.get());
            return true;
        }
    return false;
}

bool MetaPath_Playlist::GetDragDropUNCPath(Item* pItem, std::string& uncPath)
{
    if (auto p = dynamic_cast<ItemMegaNode*>(pItem))
    {
        OwningAccountPtr ap;
        if (auto n = g_mega->findNode(p->mnode->getHandle(), &ap))
        {
            uncPath = PlatformMegaUNCPrefix(ap->masp.get()) + ap->masp->getNodePath(n.get());
            return true;
        }
    }
    return false;
}

string MetaPath_LocalFS::GetFullPath(Item& item) const
{
    return (localPath / fs::u8path(item.u8Name)).u8string();
}

string MetaPath_MegaFS::GetFullPath(Item& item) const
{
    if (auto p = dynamic_cast<ItemMegaNode*>(&item))
        if (auto ap = wap.lock())
            return ap->masp->getNodePath(p->mnode.get());
    return string();
}

std::string MetaPath_MegaAccount::u8DisplayPath() const
{
    if (auto ap = wap.lock())
    {
        OwnStr e(ap->masp->getMyEmail(), false);
        return string(e ? e.get() : "<null>");
    }
    return nullptr;
}

std::unique_ptr<MetaPath> MetaPath::deserialize(const std::string& s)
{
    if (s == "TopShelf")
    {
        return std::make_unique<MetaPath_TopShelf>();
    }
    if (s.size() >= 9 && s.substr(0, 8) == "LocalFS/")
    {
        return std::make_unique<MetaPath_LocalFS>(MEGA::FromBase64(s.substr(8)));
    }
    if (s.size() >= 9 && s.substr(0, 9) == "Playlist/")
    {
        return std::make_unique<MetaPath_Playlist>(MEGA::FromBase64(s.substr(9)));
    }
    if (s == "LocalVolumes")
    {
        return std::make_unique<MetaPath_LocalVolumes>();
    }
    if (s.size() >= 13 && s.substr(0, 12) == "MegaAccount/")
    {
        auto acc = MEGA::FromBase64(s.substr(12));
        for (auto ptr : g_mega->accounts())
            if (acc == OwnString(ptr->masp->getMyEmail()))
                return std::make_unique<MetaPath_MegaAccount>(ptr);
    }
    if (s.size() >= 8 && s.substr(0, 7) == "MegaFS/")
    {
        auto n = s.find("/", 7);
        if (n+1 && n > 7)
        {
            auto acc = MEGA::FromBase64(s.substr(7, n-7));
            auto path = MEGA::FromBase64(s.substr(n+1));
            for (auto ptr : g_mega->accounts())
                if (acc == ptr->accountEmail)
                    if (auto mnode = std::unique_ptr<m::MegaNode>(ptr->masp->getNodeByPath(path.c_str())))
                        return std::make_unique<MetaPath_MegaFS>(ptr, move(mnode));
        }
    }
    return nullptr;
}


std::unique_ptr<MetaPath> MetaPath_CompareView::Ascend()
{
    auto ascend1 = view1->Ascend();
    auto ascend2 = view2->Ascend();
    if (ascend1 && ascend2)
    {
        return std::make_unique<MetaPath_CompareView>(move(ascend1), move(ascend2), differentOnly);
    }
    return nullptr;
}


std::unique_ptr<MetaPath> MetaPath_CompareView::Descend(const Item& item)
{
    if (auto p = dynamic_cast<const ItemCompareItem*>(&item))
    {
        auto descend1 = view1->Descend(*p->item1);
        auto descend2 = view2->Descend(*p->item2);
        if (descend1 && descend2)
        {
            return std::make_unique<MetaPath_CompareView>(move(descend1), move(descend2), differentOnly);
        }
    }
    return nullptr;
}
