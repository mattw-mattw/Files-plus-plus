// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <string>

#include <megaapi.h>
namespace m = ::mega;


struct Item
{
	std::string u8Name;
	Item(std::string n) : u8Name(move(n)) {}
	Item() {}
	virtual ~Item() {}
	virtual bool isFolder() const { return true; }
	virtual int64_t size() const { return -1; }
};

struct ItemLocalFS : public Item
{
	int64_t filesize;
	bool folder;
	ItemLocalFS(std::string n, int64_t s, bool f) : Item(move(n)), filesize(s), folder(f) {}
	bool isFolder() const override { return folder; }
	int64_t size() const override { return filesize; }
};

struct ItemError : public Item
{
	std::string message;
	ItemError(const std::string& s) : message(s) {}
};

struct ItemMegaAccount : public Item
{
	ItemMegaAccount(std::string n, std::shared_ptr<m::MegaApi> p) : Item(std::move(n)), masp(p) {}
	std::shared_ptr<m::MegaApi> masp;
};

struct ItemMegaNode : public Item
{
	ItemMegaNode(std::unique_ptr<m::MegaNode> n) : Item(n->getName()), mnode(std::move(n)) {}
	std::unique_ptr<m::MegaNode> mnode;
	bool isFolder() const override { return mnode->isFolder(); }
	int64_t size() const override { return isFolder() ? -1 : mnode->getSize(); }
};

struct ItemMegaInshare : public Item
{
	std::string insharepath(m::MegaNode& n, m::MegaApi& api)
	{
		std::unique_ptr<char[]> p(api.getNodePath(&n));
		return p ? std::string(p.get()) : n.getName();
	}

	ItemMegaInshare(std::unique_ptr<m::MegaNode> n, m::MegaApi& api, int64_t size, bool folder) : Item(insharepath(*n, api)), mnode(std::move(n)) {}
	std::unique_ptr<m::MegaNode> mnode;
};