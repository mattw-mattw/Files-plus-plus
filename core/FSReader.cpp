// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "FSReader.h"
#include "PlatformSupplied.h"

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

auto TopShelfReader::GetMenuActions(shared_ptr<deque<Item*>> selectedItems) -> MenuActions
{
	MenuActions ma;
	if (selectedItems->size() == 1)
	{
		if (auto account = dynamic_cast<ItemMegaAccount*>((*selectedItems)[0]))
		{
			ma.emplace_back("Log Out and remove local cache", [masp = account->masp]() { g_mega->logoutremove(masp); });
		}
	}
	else if (selectedItems->empty())
	{
		ma.emplace_back("Add MEGA Account", []() { AddMEGAAccount(); });
	}
	return ma;
};


MegaAccountReader::MegaAccountReader(std::shared_ptr<m::MegaApi> p, QueueTrigger t, bool r)
	: FSReader(t, r)
	, masp(p)
	, workerthread([this]() { Threaded(); })
{
}

MegaAccountReader::~MegaAccountReader()
{
	cancelling = true;
	workerthread.join();
}

auto MegaAccountReader::GetMenuActions(std::shared_ptr<std::deque<Item*>> selectedItems) -> MenuActions
{
	return MenuActions();
}

void MegaAccountReader::Threaded()
{
	unique_ptr<m::MegaNode> root(masp->getRootNode());
	unique_ptr<m::MegaNode> inbox(masp->getInboxNode());
	unique_ptr<m::MegaNode> rubbish(masp->getRubbishNode());

	if (root) Queue(NEWITEM, make_unique<ItemMegaNode>(move(root)));
	if (inbox) Queue(NEWITEM, make_unique<ItemMegaNode>(move(inbox)));
	if (rubbish) Queue(NEWITEM, make_unique<ItemMegaNode>(move(rubbish)));

	unique_ptr<m::MegaNodeList> inshares(masp->getInShares());
	if (inshares)
	{
		for (int i = 0; i < inshares->size(); ++i)
		{
			Queue(NEWITEM, make_unique<ItemMegaInshare>(std::unique_ptr<m::MegaNode>(inshares->get(i)->copy()), *masp, -1, true));
		}
	}

	Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
	Send();
}

MegaFSReader::MegaFSReader(std::shared_ptr<m::MegaApi> p, std::unique_ptr<m::MegaNode> n, QueueTrigger t, bool r)
	: FSReader(t, r)
	, masp(p)
	, mnode(move(n))
	, workerthread([this]() { Threaded(); })
{
}

MegaFSReader::~MegaFSReader()
{
	cancelling = true;
	listener.nq.push(nullptr);
	workerthread.join();
}

void MegaFSReader::Threaded()
{
	masp->addGlobalListener(&listener);

	for (;;)
	{
		bool reload_needed = false;
		unique_ptr<m::MegaChildrenLists> mc(masp->getFileFolderChildren(mnode.get()));
		if (mc)
		{
			m::MegaNodeList* folders = mc->getFolderList();
			m::MegaNodeList* files = mc->getFileList();

			for (int i = 0; i < folders->size(); ++i)
			{
				Queue(NEWITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(folders->get(i)->copy())));
			}
			for (int i = 0; i < files->size(); ++i)
			{
				Queue(NEWITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(files->get(i)->copy())));
			}
		}
		Queue(FILE_ACTION_APP_READCOMPLETE, NULL);
		Send();

		while (!cancelling && !reload_needed)
		{
			unique_ptr<m::MegaNodeList> nodes;
			if (listener.nq.pop(nodes))
			{
				if (cancelling) break;
				if (!nodes)
				{
					// MEGA callback says to reload, due to too many changes
					Queue(FILE_ACTION_APP_READRESTARTED, NULL);
					reload_needed = true;
					break;
				}
				for (int i = 0; i < nodes->size(); ++i)
				{
					auto n = nodes->get(i);
					if (n->getParentHandle() == mnode->getHandle() && n->isFolder() || n->isFile())
					{
						if (n->hasChanged(m::MegaNode::CHANGE_TYPE_REMOVED))
						{
							Queue(DELETEDITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(n->copy())));
						}
						else if (n->hasChanged(m::MegaNode::CHANGE_TYPE_NEW))
						{
							Queue(NEWITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(n->copy())));
						}
						else if (n->hasChanged(m::MegaNode::CHANGE_TYPE_ATTRIBUTES)) // could be renamed
						{
							Queue(DELETEDITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(n->copy())));
							Queue(NEWITEM, make_unique<ItemMegaNode>(std::unique_ptr<m::MegaNode>(n->copy())));
						}
					}
				}
				Send();
			}
		}
		if (!reload_needed) break;
	}
	masp->removeGlobalListener(&listener);
}
