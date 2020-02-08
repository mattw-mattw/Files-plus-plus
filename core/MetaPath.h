// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include "basictypes.h"
#include "FSReader.h"
#include "PlatformSupplied.h"

struct Item;

struct MetaPath
{
    //enum PathType { None, TopShelf, LocalVolumes, LocalFS, MegaAccount, MegaChats, MegaChat, MegaFS, CommandHistory, Playlist };

	virtual bool operator==(const MetaPath& o) const = 0;
	virtual std::unique_ptr<MetaPath> clone() const = 0;
	virtual std::unique_ptr<MetaPath> Ascend() = 0;
	virtual std::unique_ptr<MetaPath> Descend(const Item&) = 0;
	virtual std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const = 0;

	virtual std::string u8DisplayPath() const = 0;

    virtual std::string serialize() = 0;
    static std::unique_ptr<MetaPath> deserialize(const std::string&);

    //operator bool() { return pathType != None; }
    //PathType getPathType() { return pathType; }
    
	virtual OwningAccountPtr Account() { return nullptr; }
	virtual bool GetDragDropUNCPath(Item*, std::string& uncPath) { return false; }
	virtual std::string GetFullPath(Item&) { return std::string(); }



//private:
//    PathType pathType = None;
//    std::filesystem::path localPath;
//	WeakAccountPtr wap;
//    copy_ptr<m::MegaNode> mnode;
//	copy_ptr<c::MegaChatRoom> chatroom;
};

struct MetaPath_TopShelf : MetaPath
{

	bool operator==(const MetaPath& o) const override { return dynamic_cast<const MetaPath_TopShelf*>(&o); }
	std::unique_ptr<MetaPath> clone() const override { return std::make_unique<MetaPath_TopShelf>(); }
	std::unique_ptr<MetaPath> Ascend() override { return nullptr; }
	std::unique_ptr<MetaPath> Descend(const Item&) override;
	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override { return std::make_unique<TopShelfReader>(t, recurse, uf); }
	std::string u8DisplayPath() const override {  return "<Top Shelf>"; }
	std::string serialize() override { return "TopShelf"; }
};


struct MetaPath_LocalVolumes : MetaPath
{
	bool operator==(const MetaPath& o) const override { return dynamic_cast<const MetaPath_LocalVolumes*>(&o); }
	std::unique_ptr<MetaPath> clone() const override { return std::make_unique<MetaPath_LocalVolumes>(); }
	std::unique_ptr<MetaPath> Ascend() override { return std::make_unique<MetaPath_TopShelf>(); }
	std::unique_ptr<MetaPath> Descend(const Item&) override;
	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override { return std::make_unique<LocalVolumeReader>(t, recurse, uf); }
	std::string u8DisplayPath() const override { return "<Local Volumes>"; }
	std::string serialize() override { return "LocalVolumes"; }
};

struct MetaPath_LocalFS : MetaPath
{
	MetaPath_LocalFS(std::filesystem::path p) : localPath(p) {}

	bool operator==(const MetaPath& o) const override { auto mp = dynamic_cast<const MetaPath_LocalFS*>(&o); return mp && mp->localPath == localPath; }
	std::unique_ptr<MetaPath> clone() const override { return std::make_unique<MetaPath_LocalFS>(localPath); }
	std::unique_ptr<MetaPath> Ascend() override;
	std::unique_ptr<MetaPath> Descend(const Item&) override;
	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override { return NewLocalFSReader(localPath, normalizeSeparator, t, recurse, uf); }
	std::string u8DisplayPath() const override { return localPath.u8string(); }
	std::string serialize() override { return  "LocalFS/" + ToBase64(localPath.u8string()); }

	std::string GetFullPath(Item&) override;
	bool GetDragDropUNCPath(Item*, std::string& uncPath) override;

	bool normalizeSeparator = false;

private:
	std::filesystem::path localPath;
};

struct MetaPath_Playlist : MetaPath
{
	MetaPath_Playlist(std::filesystem::path p) : localPath(p) {}

	bool operator==(const MetaPath& o) const override { auto mp = dynamic_cast<const MetaPath_Playlist*>(&o); return mp && mp->localPath == localPath; }
	std::unique_ptr<MetaPath> clone() const override { return std::make_unique<MetaPath_Playlist>(localPath); }
	std::unique_ptr<MetaPath> Ascend() override { return std::make_unique<MetaPath_LocalFS>(localPath.parent_path()); }
	std::unique_ptr<MetaPath> Descend(const Item&) override { return nullptr; }
	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override { return std::make_unique<PlaylistReader>(localPath, t, recurse, uf); }
	std::string u8DisplayPath() const override { return localPath.u8string(); }
	std::string serialize() override { return  "Playlist/" + ToBase64(localPath.u8string()); }

	bool GetDragDropUNCPath(Item*, std::string& uncPath) override;

//private:
	std::filesystem::path localPath;
};


struct MetaPath_MegaAccount : MetaPath
{
	MetaPath_MegaAccount(OwningAccountPtr oap) : wap(oap) {}

	bool operator==(const MetaPath& o) const override { auto ap = wap.lock(); auto mp = dynamic_cast<const MetaPath_MegaAccount*>(&o); return ap && mp && mp->wap.lock() == ap; }
	std::unique_ptr<MetaPath> clone() const override { if (auto ap = wap.lock()) return std::make_unique<MetaPath_MegaAccount>(ap); else return nullptr; }
	std::unique_ptr<MetaPath> Ascend() override { return std::make_unique<MetaPath_TopShelf>(); }
	std::unique_ptr<MetaPath> Descend(const Item&) override;
	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override { if (auto ap = wap.lock()) return std::make_unique<MegaAccountReader>(ap, t, recurse, uf); else return nullptr; }
	std::string u8DisplayPath() const override;
	std::string serialize() override { if (auto ap = wap.lock()) return  "MegaAccount/" + ToBase64(OwnString(ap->masp->getMyEmail())); else return ""; }

	OwningAccountPtr Account() override { return OwningAccountPtr(wap); }
private:
	WeakAccountPtr wap;
};

struct MetaPath_MegaFS : MetaPath
{
	MetaPath_MegaFS(OwningAccountPtr oap, std::unique_ptr<m::MegaNode>&& n) : wap(oap), mnode(move(n)) {}

	bool operator==(const MetaPath& o) const override {  auto mp = dynamic_cast<const MetaPath_MegaFS*>(&o); return mp && mp->mnode->getHandle() == mnode->getHandle(); }
	std::unique_ptr<MetaPath> clone() const override { if (auto ap = wap.lock()) return std::make_unique<MetaPath_MegaFS>(ap, mnode.copy()); else return nullptr; }
	std::unique_ptr<MetaPath> Ascend() override;
	std::unique_ptr<MetaPath> Descend(const Item&) override;
	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override { if (auto ap = wap.lock()) return std::make_unique<MegaFSReader>(ap, mnode.copy(), t, recurse, uf); else return nullptr; }
	std::string u8DisplayPath() const override { if (auto ap = wap.lock()) return OwnString(ap->masp->getNodePath(mnode.get())); else return ""; }
	std::string serialize() override { if (auto ap = wap.lock()) return  "MegaFS/" + ToBase64(OwnString(ap->masp->getMyEmail())) + "/" + ToBase64(ap->masp->getNodePath(mnode.get())); else return ""; }

	OwningAccountPtr Account() override { return OwningAccountPtr(wap); }
	std::string GetFullPath(Item&) override;
	bool GetDragDropUNCPath(Item*, std::string& uncPath) override;

private:
	WeakAccountPtr wap;
	copy_ptr<m::MegaNode> mnode;
};


struct MetaPath_MegaChats : MetaPath
{
	MetaPath_MegaChats(OwningAccountPtr oap) : wap(oap) {}

	bool operator==(const MetaPath& o) const override { auto ap = wap.lock(); auto mp = dynamic_cast<const MetaPath_MegaChats*>(&o); return ap && mp && mp->wap.lock() == ap; }
	std::unique_ptr<MetaPath> clone() const override { if (auto ap = wap.lock()) return std::make_unique<MetaPath_MegaChats>(ap); else return nullptr; }
	std::unique_ptr<MetaPath> Ascend() override { return std::make_unique<MetaPath_MegaAccount>(wap.lock()); }
	std::unique_ptr<MetaPath> Descend(const Item&) override;
	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override { if (auto ap = wap.lock()) return std::make_unique<MegaChatRoomsReader>(ap, t, recurse, uf); else return nullptr; }
	std::string u8DisplayPath() const override {return "<Chatrooms>"; }
	std::string serialize() override { if (auto ap = wap.lock()) return  "MegaChats/" + ToBase64(OwnString(ap->masp->getMyEmail())); else return ""; }

	OwningAccountPtr Account() override { return OwningAccountPtr(wap); }

private:
	WeakAccountPtr wap;
};


struct MetaPath_MegaChat : MetaPath
{
	MetaPath_MegaChat(OwningAccountPtr oap, std::unique_ptr<c::MegaChatRoom> cr) : wap(oap), chatroom(move(cr)) {}

	bool operator==(const MetaPath& o) const override {  auto mp = dynamic_cast<const MetaPath_MegaChat*>(&o); return mp && mp->chatroom->getChatId() == chatroom->getChatId(); }
	std::unique_ptr<MetaPath> clone() const override { if (auto ap = wap.lock()) return std::make_unique<MetaPath_MegaChat>(ap, chatroom.copy()); else return nullptr; }
	std::unique_ptr<MetaPath> Ascend() override { return std::make_unique<MetaPath_MegaChats>(wap.lock()); }
	std::unique_ptr<MetaPath> Descend(const Item&) override { return nullptr; }
	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override { if (auto ap = wap.lock()) return std::make_unique<MegaChatReader>(ap, std::unique_ptr<c::MegaChatRoom>(chatroom->copy()), t, recurse, uf); else return nullptr; }
	std::string u8DisplayPath() const override {  return "<Chat>"; }
	std::string serialize() override { if (auto ap = wap.lock()) return  "MegaChat/" + ToBase64(OwnString(ap->masp->getMyEmail())) + "/" + ToBase64_H8(chatroom->getChatId()); else return ""; }

	OwningAccountPtr Account() override { return OwningAccountPtr(wap); }

private:
	WeakAccountPtr wap;
	copy_ptr<c::MegaChatRoom> chatroom;
};


struct MetaPath_CommandHistory : MetaPath
{
	MetaPath_CommandHistory(OwningAccountPtr oap) : wap(oap) {}

	bool operator==(const MetaPath& o) const override { auto ap = wap.lock(); auto mp = dynamic_cast<const MetaPath_CommandHistory*>(&o); return ap && mp && mp->wap.lock() == ap; }
	std::unique_ptr<MetaPath> clone() const override { if (auto ap = wap.lock()) return std::make_unique<MetaPath_CommandHistory>(ap); else return nullptr; }
	std::unique_ptr<MetaPath> Ascend() override { return std::make_unique<MetaPath_TopShelf>(); }
	std::unique_ptr<MetaPath> Descend(const Item&) override { return nullptr; }
	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override { return std::make_unique<CommandHistoryReader>(t, uf); }
	std::string u8DisplayPath() const override { return "<Command History>"; }
	std::string serialize() override { return ""; }

	OwningAccountPtr Account() override { return OwningAccountPtr(wap); }

private:
	WeakAccountPtr wap;
};


struct MetaPath_CompareView : MetaPath
{
	MetaPath_CompareView(std::unique_ptr<MetaPath> v1, std::unique_ptr<MetaPath> v2, bool diffonly) : view1(move(v1)), view2(move(v2)), differentOnly(diffonly) {}

	bool operator==(const MetaPath& o) const override { auto mp = dynamic_cast<const MetaPath_CompareView*>(&o); return mp && *mp->view1 == *view1 && *mp->view2 == *view2; }
	std::unique_ptr<MetaPath> clone() const override { return std::make_unique<MetaPath_CompareView>(view1->clone(), view2->clone(), differentOnly); }
	std::unique_ptr<MetaPath> Ascend() override { return nullptr; }
	std::unique_ptr<MetaPath> Descend(const Item&) override { return nullptr; }
	std::string u8DisplayPath() const override { return view1->u8DisplayPath() + " vs " + view2->u8DisplayPath() + (differentOnly ? " (different only)": ""); }
	std::string serialize() override { return ""; }

	std::unique_ptr<FSReader> GetContentReader(QueueTrigger t, bool recurse, UserFeedback& uf) const override 
	{ 
		return std::make_unique<CompareViewReader>(*view1, *view2, differentOnly, t, recurse, uf); 
	}

private:
	std::unique_ptr<MetaPath> view1;
	std::unique_ptr<MetaPath> view2;
	bool differentOnly;
};




class Favourites
{
	std::vector<std::unique_ptr<MetaPath>> favourites;
	std::mutex m;

public:
	std::vector<std::unique_ptr<MetaPath>> copy()
	{
		std::lock_guard g(m);
		std::vector<std::unique_ptr<MetaPath>> v;
		for (auto& f : favourites) v.emplace_back(f->clone());
		return v;
	}

	bool Toggle(const MetaPath& p)
	{
		std::lock_guard g(m);
		for (unsigned i = 0; i < favourites.size(); ++i)
		{
			if (favourites[i]->u8DisplayPath() == p.u8DisplayPath())
			{
				favourites.erase(favourites.begin() + i);
				return false;
			}
		}
		favourites.push_back(p.clone());
		return true;
	}
};

