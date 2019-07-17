// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "MetaPath.h"
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
		else
		{
			pathType = LocalVolumes;
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
