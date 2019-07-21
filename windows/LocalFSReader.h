// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "../core/FSReader.h"

struct OVERLAPPED0 : public OVERLAPPED
{
	OVERLAPPED0() { memset(this, 0, sizeof(*this)); }
};

class LocalFSReader : public FSReader
{
	HANDLE hDirectory = INVALID_HANDLE_VALUE;
	OVERLAPPED0 overlapped;
	std::string overlappedbuf;
	bool cancelling = false;
    HWND hwnd= 0;

public:
	std::filesystem::path dir;

	LocalFSReader(std::filesystem::path p, QueueTrigger, bool r);
	~LocalFSReader();

	MenuActions GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) override;

private:

    void OnDragDroppedMEGAItems(MEGA::ApiPtr masp, const std::deque<std::unique_ptr<m::MegaNode>>& nodes) override;
    void OnDragDroppedLocalItems(const std::deque<std::filesystem::path>& paths) override;


	bool RequestChanges();
	bool ReadDir(const std::filesystem::path& p, bool recurse, const std::filesystem::path& recurseprefix);
	void Threaded();

	std::thread workerthread;
};