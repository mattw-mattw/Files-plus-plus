// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include "MEGA.h"
#include "Item.h"

struct FSReader
{
	enum Action {
		FILE_ACTION_APP_ERROR, FILE_ACTION_APP_NOTIFY_FAILURE, FILE_ACTION_APP_READCOMPLETE, FOLDER_RESOLVED_SOFTLINK,
		NEWITEM, DELETEDITEM, RENAMEDFROM, RENAMEDTO, INVALID_ACTION
	};

	typedef std::function<void()> QueueTrigger;

	FSReader(QueueTrigger t, bool r) : trigger(t), recurse(r) {}
	virtual ~FSReader() {}

	typedef std::vector<std::unique_ptr<Item>> Batch;
	struct Entry { Action action; Batch batch; };
	NotifyQueue<Entry> q;

	// todo: convert to getter virtual void AddMenuItems(CMenu& m, int& id, std::map<int, std::function<void()>>& me) {}

protected:
	bool recurse = false;

	Action currentaction = INVALID_ACTION;
	Batch currentitems;
	QueueTrigger trigger;

	void Send();
	void Queue(Action action, std::unique_ptr<Item>&& p);
};


struct TopShelfReader : public FSReader
{
	TopShelfReader(QueueTrigger t, bool r);
	~TopShelfReader();

	//void AddMenuItems(CMenu& m, int& id, std::map<int, std::function<void()>>& me) override;

private:
	void Threaded();

	bool cancelling = false;
	std::thread workerthread;
};


class LocalVolumeReader : public FSReader
{
	bool cancelling = false;

public:
	LocalVolumeReader(QueueTrigger t, bool r);
	~LocalVolumeReader();

private:
	void Threaded();

	std::thread workerthread;
};


struct MegaAccountReader : public FSReader
{
	MegaAccountReader(std::shared_ptr<m::MegaApi> p, QueueTrigger t, bool r);
	~MegaAccountReader();

private:
	void Threaded();

	std::shared_ptr<m::MegaApi> masp;
	bool cancelling = false;
	std::thread workerthread;
};


struct MegaFSReader : public FSReader
{
	MegaFSReader(std::shared_ptr<m::MegaApi> p, std::unique_ptr<m::MegaNode>, QueueTrigger t, bool r);
	~MegaFSReader();

private:
	void Threaded();

	NodeUpdateListener listener;
	std::shared_ptr<m::MegaApi> masp;
	std::unique_ptr<m::MegaNode> mnode;
	bool cancelling = false;
	std::thread workerthread;
};

