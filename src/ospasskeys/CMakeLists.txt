#  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 3 of the License, or
#  (at your option) any later version.

if(NOT KPXC_FEATURE_OS_PASSKEYS)
    set(ospasskeys_LIB "" PARENT_SCOPE)
    return()
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR})

set(ospasskeys_SOURCES
        CtapCodec.cpp
        OsPasskeyCeremony.cpp)

if(WIN32)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/windows)
    list(APPEND ospasskeys_SOURCES
            windows/OsPasskeyBootstrap.cpp
            windows/OsPasskeyService.cpp
            windows/PluginAuthenticatorImpl.cpp
            windows/PluginRegistrationManager.cpp)
    add_library(ospasskeys STATIC ${ospasskeys_SOURCES})
    target_link_libraries(ospasskeys
            passkeys
            Qt6::Core
            Qt6::Widgets
            ole32
            oleaut32
            uuid
            kernel32
            webauthn)
    target_include_directories(ospasskeys PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/windows)
elseif(UNIX AND NOT APPLE AND NOT HAIKU)
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/linux)
    list(APPEND ospasskeys_SOURCES
            linux/OsPasskeyBootstrap.cpp
            linux/OsPasskeyService.cpp
            linux/UhidCtapDevice.cpp)
    add_library(ospasskeys STATIC ${ospasskeys_SOURCES})
    target_link_libraries(ospasskeys
            passkeys
            Qt6::Core
            Qt6::Widgets)
    target_include_directories(ospasskeys PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/linux)
else()
    set(ospasskeys_LIB "" PARENT_SCOPE)
    return()
endif()

set(ospasskeys_LIB ospasskeys PARENT_SCOPE)
