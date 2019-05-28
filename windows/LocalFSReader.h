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

public:
	std::filesystem::path dir;

	LocalFSReader(std::filesystem::path p, QueueTrigger, bool r);
	~LocalFSReader();

private:
	bool RequestChanges();
	bool ReadDir(const std::filesystem::path& p, bool recurse, const std::filesystem::path& recurseprefix);
	void Threaded();

	std::thread workerthread;
};