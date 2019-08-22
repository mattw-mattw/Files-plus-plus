// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include "basictypes.h"
#include "Item.h"

struct FSReader
{
	enum Action {
		FILE_ACTION_APP_ERROR, FILE_ACTION_APP_NOTIFY_FAILURE, FILE_ACTION_APP_READCOMPLETE, FILE_ACTION_APP_READRESTARTED, FOLDER_RESOLVED_SOFTLINK,
		NEWITEM, DELETEDITEM, RENAMEDFROM, RENAMEDTO, INVALID_ACTION
	};

	typedef std::function<void()> QueueTrigger;

	FSReader(QueueTrigger t, bool r) : trigger(t), recurse(r) {}
	virtual ~FSReader() {}

	typedef std::vector<std::unique_ptr<Item>> Batch;
	struct Entry { Action action; Batch batch; };
	NotifyQueue<Entry> q;

	virtual MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) { return MenuActions(); }

    virtual void OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths);
    virtual void OnDragDroppedMEGAItems(ApiPtr masp, const std::deque<std::unique_ptr<m::MegaNode>>& nodes);

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

	MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) override;

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

	MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) override;

private:
	void Threaded();

	std::shared_ptr<m::MegaApi> masp;
	bool cancelling = false;
	std::thread workerthread;
};

struct NodeUpdateListener : public m::MegaGlobalListener
{
    NotifyQueue<std::unique_ptr<m::MegaNodeList>> nq;

    void onNodesUpdate(m::MegaApi*, m::MegaNodeList* nodes) override
    {
        nq.push(std::unique_ptr<m::MegaNodeList>(nodes ? nodes->copy() : nullptr));
    }
};

struct MegaFSReader : public FSReader
{
	MegaFSReader(std::shared_ptr<m::MegaApi> p, std::unique_ptr<m::MegaNode>, QueueTrigger t, bool r);
	~MegaFSReader();

private:
	void Threaded();
    void RecursiveRead(m::MegaNode& mnode, const std::string& basepath);
    void OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths) override;
    void OnDragDroppedMEGAItems(ApiPtr masp, const std::deque<std::unique_ptr<m::MegaNode>>& nodes) override;
    auto GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems)->MenuActions override;


	NodeUpdateListener listener;
	std::shared_ptr<m::MegaApi> masp;
	std::unique_ptr<m::MegaNode> mnode;
    std::set<::mega::MegaHandle> nodes_present;
	bool cancelling = false;
	std::thread workerthread;
};


struct CommandHistoryReader : public FSReader
{
    CommandHistoryReader(QueueTrigger t);
    ~CommandHistoryReader();

    MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) override;

private:
    void Threaded();

    bool cancelling = false;
    std::thread workerthread;
};
