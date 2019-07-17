// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include "basictypes.h"
#include "Item.h"
#include "FSReader.h"

class MetaPath
{
	enum PathType { None, TopShelf, LocalVolumes, LocalFS, MegaAccount, MegaFS };
	PathType pathType = None;
	std::filesystem::path localPath;
	std::shared_ptr<m::MegaApi> masp;
	copy_ptr<m::MegaNode> mnode;
public:
	bool operator==(const MetaPath& o) const;

	bool Ascend();
	bool Descend(const Item&);

	void SetLocalPath(const std::filesystem::path& p);
	bool GetLocalPath(std::filesystem::path& p) const;

    std::string GetFullPath(Item&);

	std::unique_ptr<FSReader> GetContentReader(FSReader::QueueTrigger t, bool recurse) const;

	std::string u8DisplayPath() const;

	std::function<bool(std::unique_ptr<Item>& a, std::unique_ptr<Item>& b)> nodeCompare();
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

extern Favourites g_favourites;
