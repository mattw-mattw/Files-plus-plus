// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include "basictypes.h"
#include "FSReader.h"

struct Item;

struct Account
{
    std::filesystem::path cacheFolder;
    OwningApiPtr masp;
    Account(const std::filesystem::path& p, OwningApiPtr ap) : cacheFolder(p), masp(ap) {}
};

struct PublicFolder : Account
{
    std::string folderLink;
    PublicFolder(const std::string& link, const std::filesystem::path& p, OwningApiPtr masp) : Account(p, masp), folderLink(link) {}
    ~PublicFolder() {}
};

typedef std::shared_ptr<Account> AccountPtr;
typedef std::shared_ptr<PublicFolder> FolderPtr;


class MetaPath
{
	enum PathType { None, TopShelf, LocalVolumes, LocalFS, MegaAccount, MegaFS, CommandHistory};
	PathType pathType = None;
	std::filesystem::path localPath;
	ApiPtr mawp;
	copy_ptr<m::MegaNode> mnode;
public:
	bool operator==(const MetaPath& o) const;

	bool Ascend();
	bool Descend(const Item&);

	void SetLocalPath(const std::filesystem::path& p);
	bool GetLocalPath(std::filesystem::path& p) const;

    bool GetDragDropUNCPath(Item*, std::string& uncPath);

    std::string GetFullPath(Item&);

	std::unique_ptr<FSReader> GetContentReader(FSReader::QueueTrigger t, bool recurse, UserFeedback& uf) const;

	std::string u8DisplayPath() const;

	std::function<bool(std::unique_ptr<Item>& a, std::unique_ptr<Item>& b)> nodeCompare();

    bool serialize(std::string&);
    static MetaPath deserialize(const std::string&);

    operator bool() { return pathType != None; }
    OwningApiPtr Account();
};


class Favourites
{
	std::vector<MetaPath> favourites;
	std::mutex m;

public:
	std::vector<MetaPath> copy()
	{
		std::lock_guard g(m);
		return favourites;
	}

	bool Toggle(const MetaPath& p)
	{
		std::lock_guard g(m);
		for (unsigned i = 0; i < favourites.size(); ++i)
		{
			if (favourites[i].u8DisplayPath() == p.u8DisplayPath())
			{
				favourites.erase(favourites.begin() + i);
				return false;
			}
		}
		favourites.push_back(p);
		return true;
	}
};

