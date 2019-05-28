# Copyright 2019 The Files++ Authors.  Licence via 2-Clause BSD, see the LICENSE file.

cmake_minimum_required(VERSION 3.14)
project("Files++")

#specify where 3rd party libraries are available
set(Mega3rdPartyDir "${CMAKE_CURRENT_LIST_DIR}/../3rdParty"  CACHE STRING "Third party library location")

# specify the MEGA SDK location 
include(${CMAKE_CURRENT_LIST_DIR}/MEGA.nz/sdk/contrib/cmake/CMakeLists.txt)

if (WIN32)

	set (CMAKE_MFC_FLAG 1)  # 1: use MFC as a static library

	add_executable("Files++" 
			core/MEGA.cpp core/MEGA.h
			core/FSReader.cpp core/FSReader.h
			core/MetaPath.cpp core/MetaPath.h
			windows/Files++.cpp windows/Files++.h
			windows/Files++.rc 
			windows/Files++Dlg.cpp windows/Files++Dlg.h
			windows/FilterDlg.cpp windows/FilterDlg.h
			windows/LocalFSReader.cpp windows/LocalFSReader.h
			windows/stdafx.cpp windows/stdafx.h
			windows/targetver.h
			windows/UserPassDialog.cpp windows/UserPassDialog.h
			windows/PlatformSupplied.cpp
	)
	target_link_libraries("Files++" PUBLIC Mega)

	set_target_properties("Files++"  PROPERTIES
		COMPILE_DEFINITIONS _UNICODE,UNICODE,_BIND_TO_CURRENT_CRT_VERSION,_BIND_TO_CURRENT_MFC_VERSION
		LINK_FLAGS "/ENTRY:\"wWinMainCRTStartup\"")
endif(WIN32)

