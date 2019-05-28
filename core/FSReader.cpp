// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "FSReader.h"

using namespace std;

void FSReader::Send()
{
	if (!currentitems.empty())
	{
		q.push(Entry{ currentaction, move(currentitems) });
		currentitems.clear();
		if (trigger) trigger();
	}
	currentaction = INVALID_ACTION;
}

void FSReader::Queue(Action action, unique_ptr<Item>&& p)
{
	if (currentitems.size() >= 100 || (currentaction != action && !(action == RENAMEDFROM && currentaction == RENAMEDFROM)))
	{
		Send();
	}
	currentaction = action;
	currentitems.emplace_back(move(p));
}

TopShelfReader::TopShelfReader(QueueTrigger t, bool r)
	: FSReader(t, r)
	, workerthread([this]() { Threaded(); })
{
}

TopShelfReader::~TopShelfReader()
{
	cancelling = true;
	workerthread.join();
}

void TopShelfReader::Threaded()
{
	Queue(NEWITEM, make_unique<Item>("<Local Volumes>"));
	auto megaAccounts = g_mega->accounts();
	for (auto& p : megaAccounts)
	{
		unique_ptr<char[]> email(p->getMyEmail());
		Queue(NEWITEM, make_unique<ItemMegaAccount>(email.get() ? email.get() : "<loading>", p));
	}
	Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
	Send();
}

//void TopShelfReader::AddMenuItems(CMenu& m, int& id, map<int, std::function<void()>>& me)
//{
//	m.AppendMenu(MF_SEPARATOR, id++, LPCTSTR(nullptr));
//
//	m.AppendMenu(MF_STRING, id, _T("Add MEGA Account"));
//	me[id] = [this]() { AddMEGAAccount();  };
//	++id;
//}
//