// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

#include <sstream>
#include "basictypes.h"
#include "Item.h"

struct MetaPath;

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

typedef std::function<void()> QueueTrigger;

enum Action {
	FILE_ACTION_APP_ERROR, FILE_ACTION_APP_NOTIFY_FAILURE, FILE_ACTION_APP_READCOMPLETE, FILE_ACTION_APP_READRESTARTED, FOLDER_RESOLVED_SOFTLINK,
	NEWITEM, DELETEDITEM, RENAMEDFROM, RENAMEDTO, INVALID_ACTION
};

struct ItemQueue
{
	typedef std::deque<std::unique_ptr<Item>> Batch;
	struct Entry { Action action; Batch batch; };
	NotifyQueue<Entry> q;

	Action currentaction = INVALID_ACTION;
	Batch currentitems;
	QueueTrigger trigger;

	void Send();
	void Queue(Action action, std::unique_ptr<Item>&& p);

	ItemQueue(QueueTrigger t) : trigger(t) {}
};

struct ColumnDef
{
	std::string name;
	unsigned width;
	std::function<std::string(const Item*)> valueFunc = [](const Item* i){ return i->u8Name; };
	std::function<int64_t(const Item*)> intFunc = nullptr;
	std::function<int64_t(const Item*)> initialSort = nullptr;
};

struct FSReader
{
	FSReader(QueueTrigger t, bool r, UserFeedback& uf) : itemQueue(new ItemQueue(t)), recurse(r), userFeedback(uf) {}
	virtual ~FSReader() {}

	// gui thread functions
    virtual MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) { return MenuActions(); }
    virtual void OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths);
    virtual void OnDragDroppedMEGAItems(OwningApiPtr masp, const std::deque<std::unique_ptr<m::MegaNode>>& nodes);

	std::shared_ptr<ItemQueue> itemQueue;

	std::vector<ColumnDef> columnDefs;

protected:
	bool recurse = false;

    // only use this in gui thread functions
    UserFeedback& userFeedback;
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
	MegaAccountReader(WeakAccountPtr p, QueueTrigger t, bool r, UserFeedback& uf);
	~MegaAccountReader();

	MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) override;

private:
	void Threaded();

	WeakAccountPtr wap;
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
	MegaFSReader(OwningAccountPtr p, std::unique_ptr<m::MegaNode>, QueueTrigger t, bool r, UserFeedback& uf);
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

struct MegaChatRoomsReader : public FSReader
{
	MegaChatRoomsReader(OwningAccountPtr p, QueueTrigger t, bool r, UserFeedback& uf);
	~MegaChatRoomsReader();

private:
	void Threaded();
	void RecursiveRead(m::MegaNode& mnode, const std::string& basepath);
	void OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths) override;
	void OnDragDroppedMEGAItems(OwningApiPtr masp, const std::deque<std::unique_ptr<m::MegaNode>>& nodes) override;
	auto GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems)->MenuActions override;

	NodeUpdateListener listener;
	OwningAccountPtr ap;  
	bool cancelling = false;
	std::thread workerthread;
};

struct MegaChatReader;

struct ChatRoomListener : public c::MegaChatRoomListener
{
	OwningAccountPtr ap;
	c::MegaChatHandle roomId;
	ChatRoomListener(OwningAccountPtr p, c::MegaChatHandle rid) : ap(p), roomId(rid) {}

	std::mutex m;
	std::map<c::MegaChatHandle, std::unique_ptr<c::MegaChatMessage>> messages;
	std::set<std::shared_ptr<ItemQueue>> callbacks;
	unsigned messageCallbackCount = 0;


	void onChatRoomUpdate(c::MegaChatApi* api, c::MegaChatRoom* chat) override;
	void onMessageLoaded(c::MegaChatApi* api, c::MegaChatMessage* msg) override;
	void onMessageReceived(c::MegaChatApi* api, c::MegaChatMessage* msg) override;
	void onMessageUpdate(c::MegaChatApi* api, c::MegaChatMessage* msg) override;
	void onHistoryReloaded(c::MegaChatApi* api, c::MegaChatRoom* chat) override;
	void onReactionUpdate(c::MegaChatApi* api, c::MegaChatHandle msgid, const char* reaction, int count) override;

};

struct MegaChatReader : public FSReader
{
	MegaChatReader(OwningAccountPtr p, std::unique_ptr<c::MegaChatRoom>, QueueTrigger t, bool r, UserFeedback& uf);
	~MegaChatReader();

private:
	void Threaded();
	void RecursiveRead(m::MegaNode& mnode, const std::string& basepath);
	void OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths) override;
	void OnDragDroppedMEGAItems(OwningApiPtr masp, const std::deque<std::unique_ptr<m::MegaNode>>& nodes) override;
	auto GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems)->MenuActions override;


	//NotifyQueue<std::unique_ptr<m::MegaNodeList>> nq;
	OwningAccountPtr ap;
	std::unique_ptr<c::MegaChatRoom> chatroom;
	//	std::unique_ptr<m::MegaNode> mnode;
	//	std::set<::mega::MegaHandle> nodes_present;
	bool cancelling = false;
	std::thread workerthread;

	std::shared_ptr<ChatRoomListener> listener;

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


struct CompareViewReader : FSReader
{
	QueueTrigger compare_trigger = []() {  };

	CompareViewReader(MetaPath& mp1, MetaPath& mp2, bool differentOnly, QueueTrigger t, bool r, UserFeedback& uf);
	~CompareViewReader();

private:
	void Threaded();

	std::unique_ptr<FSReader> reader1, reader2;
	bool differentOnly;
	bool cancelling = false;
	std::thread workerthread;
};



