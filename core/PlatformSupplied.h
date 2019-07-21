// Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

#pragma once

// Functions delared here can be used in core code but are defined in PlatformSpecific.cpp for each platform

#include "basictypes.h"
#include "FSReader.h"

fs::path BasePath();

void ReportError(const std::string& message, const m::MegaError* e = nullptr);

void AddMEGAAccount();
void AddMEGAFolderLink();
void RemoveMEGAAccount(std::shared_ptr<m::MegaApi>);

std::unique_ptr<FSReader> NewLocalFSReader(const fs::path& p, FSReader::QueueTrigger t, bool recurse);

bool LocalUserCrypt(std::string& data, bool encrypt);

std::string PlatformLocalUNCPrefix();
std::string PlatformMegaUNCPrefix(m::MegaApi*);


