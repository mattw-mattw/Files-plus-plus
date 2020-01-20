// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "MEGA.h"

using namespace std;

std::shared_ptr<ChatRoomListener> Account::getChatListener(c::MegaChatHandle roomid, std::shared_ptr<ItemQueue> iq)
{
    bool open = false;
    std::shared_ptr<ChatRoomListener> p;
    std::map<c::MegaChatHandle, std::unique_ptr<c::MegaChatMessage>> catchupMessages;
    {
        std::lock_guard g(m);
        p = chatListeners[roomid];
        if (!p) {
            p = chatListeners[roomid] = make_shared<ChatRoomListener>(shared_from_this(), roomid);
            open = true;
        }
        std::lock_guard gg(p->m);
        p->callbacks.insert(iq);
        if (!open)
        {
            for (auto& i : p->messages) catchupMessages[i.first].reset(i.second->copy());
        }
    }

    if (open) {
        if (mcsp->openChatRoom(roomid, p.get()))
        {
            p->messageCallbackCount = 1;
            p->onMessageLoaded(mcsp.get(), nullptr);
        }
        else assert(false);
    }
    else {
        for (auto& i : catchupMessages) {
            iq->Queue(NEWITEM, std::make_unique<ItemMegaChatMessage>(move(i.second)));
        }
        iq->Send();
    }
    return p;
}

void Account::deregisterFromChatListener(c::MegaChatHandle roomid, std::shared_ptr<ItemQueue> iq)
{
    std::lock_guard g(m);
    if (auto& p = chatListeners[roomid]) {
        std::lock_guard gg(p->m);
        p->callbacks.erase(iq);
    }
}


ItemCommand::ItemCommand(std::shared_ptr<MRequest> r) 
    : Item(r->getAction() + ": " + r->getState()) 
{
}

ItemMegaChatMessage::ItemMegaChatMessage(std::unique_ptr<c::MegaChatMessage>&& m) 
    : message(move(m))
{
    const char* x = message->getContent();
    if (!x) x = "<NULL>";
    u8Name = std::to_string(message->getMsgIndex()) + ":" + to_string(message->getUserHandle()) + ": " + string(x);

}
