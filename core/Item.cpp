// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#include "MEGA.h"

ItemCommand::ItemCommand(std::shared_ptr<MRequest> r) 
    : Item(r->getAction() + ": " + r->getState()) 
{
}
