// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <sstream>
#include "basictypes.h"
#include "Item.h"

struct UserFeedback
{
    void SetUserFeedback(const std::ostringstream& s);
    virtual void SetUserFeedbackStr(const std::string& s);
    virtual void SetUserFeedbackCStr(const char* s) = 0;

    virtual void ClearFilteredItems() = 0;
    virtual void AddFilteredItem(Item*) = 0;
    virtual void DoneAddingFilteredItems() = 0;
    virtual void SetFilterText(const std::string& s) = 0;
};

struct FSReader
{
	enum Action {
		FILE_ACTION_APP_ERROR, FILE_ACTION_APP_NOTIFY_FAILURE, FILE_ACTION_APP_READCOMPLETE, FILE_ACTION_APP_READRESTARTED, FOLDER_RESOLVED_SOFTLINK,
		NEWITEM, DELETEDITEM, RENAMEDFROM, RENAMEDTO, INVALID_ACTION
	};

	typedef std::function<void()> QueueTrigger;

	FSReader(QueueTrigger t, bool r, UserFeedback& uf) : trigger(t), recurse(r), userFeedback(uf) {}
	virtual ~FSReader() {}

	typedef std::vector<std::unique_ptr<Item>> Batch;
	struct Entry { Action action; Batch batch; };
	NotifyQueue<Entry> q;

	// gui thread functions
    virtual MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) { return MenuActions(); }
    virtual void OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths);
    virtual void OnDragDroppedMEGAItems(OwningApiPtr masp, const std::deque<std::unique_ptr<m::MegaNode>>& nodes);

protected:
	bool recurse = false;

	Action currentaction = INVALID_ACTION;
	Batch currentitems;
	QueueTrigger trigger;

    // only use this in gui thread functions
    UserFeedback& userFeedback;

	void Send();
	void Queue(Action action, std::unique_ptr<Item>&& p);
};


struct TopShelfReader : public FSReader
{
	TopShelfReader(QueueTrigger t, bool r, UserFeedback& uf);
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
	LocalVolumeReader(QueueTrigger t, bool r, UserFeedback& uf);
	~LocalVolumeReader();

private:
	void Threaded();

	std::thread workerthread;
};


struct MegaAccountReader : public FSReader
{
	MegaAccountReader(ApiPtr p, QueueTrigger t, bool r, UserFeedback& uf);
	~MegaAccountReader();

	MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) override;

private:
	void Threaded();

	ApiPtr mawp;
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
	MegaFSReader(ApiPtr p, std::unique_ptr<m::MegaNode>, QueueTrigger t, bool r, UserFeedback& uf);
	~MegaFSReader();

private:
	void Threaded();
    void RecursiveRead(m::MegaNode& mnode, const std::string& basepath);
    void OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths) override;
    void OnDragDroppedMEGAItems(OwningApiPtr masp, const std::deque<std::unique_ptr<m::MegaNode>>& nodes) override;
    auto GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems)->MenuActions override;


	NodeUpdateListener listener;
	OwningApiPtr masp;  // TODO: make sure we destory the reader on window close
	std::unique_ptr<m::MegaNode> mnode;
    std::set<::mega::MegaHandle> nodes_present;
	bool cancelling = false;
	std::thread workerthread;
};


struct CommandHistoryReader : public FSReader
{
    CommandHistoryReader(QueueTrigger t, UserFeedback& uf);
    ~CommandHistoryReader();

    MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) override;

private:
    void Threaded();

    bool cancelling = false;
    std::thread workerthread;
};

struct PlaylistReader : public FSReader
{
    PlaylistReader(const std::filesystem::path&, QueueTrigger t, bool r, UserFeedback& uf);
    ~PlaylistReader();

    MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) override;

private:
    void Threaded();
    void OnDragDroppedMEGAItems(OwningApiPtr source_masp, const std::deque<std::unique_ptr<m::MegaNode>>& nodes) override;

    std::filesystem::path fullFilePath;
    bool cancelling = false;
    std::thread workerthread;
};

