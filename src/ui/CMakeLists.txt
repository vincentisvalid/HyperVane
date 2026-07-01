cmake_minimum_required(VERSION 3.22)
project(HyperVaneUI VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Widgets
    Multimedia
    MultimediaWidgets
    Network
    Svg
)

find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)

# Prefix all source/header paths with CMAKE_CURRENT_SOURCE_DIR so that
# Qt6's qt_add_executable (which resolves relative paths against the
# binary dir when sources pass through function arguments) finds them
# correctly in both dev and installed/Nix builds.
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/MainWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LibraryView.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/PreviewPlayer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GameGridDelegate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/IPCClient.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/IntroOverlay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/AccountManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/LoginDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RegisterDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/VirtualKeyboard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GameArtLoader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/EmulatorCoreConfig.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ControllerWizard.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/UISoundManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ControllerInput.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ThemeEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SocoliteEngine.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SocoliteFriendsList.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SocoliteChat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SocoliteCanvas.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SocoliteAvatarEditor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SocoliteUserSearch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/SocoliteActivityFeed.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RomDownloader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/CoreManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/InGameMenu.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/RomHasher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/GameDetailDialog.cpp
)

set(HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/MainWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/LibraryView.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/PreviewPlayer.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GameGridDelegate.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/IPCClient.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/IntroOverlay.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/AccountManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/LoginDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/RegisterDialog.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/VirtualKeyboard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GameArtLoader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/EmulatorCoreConfig.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ControllerWizard.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/UISoundManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ControllerInput.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/ThemeEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SocoliteTypes.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SocoliteEngine.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SocoliteFriendsList.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SocoliteChat.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SocoliteCanvas.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SocoliteAvatarEditor.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SocoliteUserSearch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/SocoliteActivityFeed.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/RomDownloader.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/CoreManager.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/InGameMenu.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/RomHasher.h
    ${CMAKE_CURRENT_SOURCE_DIR}/include/GameDetailDialog.h
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    ${CMAKE_CURRENT_SOURCE_DIR}/../../proto/hypervane.proto
)

qt_add_executable(hypervane-ui
    ${SOURCES}
    ${HEADERS}
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    ${CMAKE_CURRENT_SOURCE_DIR}/resources/resources.qrc
)

target_include_directories(hypervane-ui PRIVATE
    include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${Protobuf_INCLUDE_DIRS}
)

target_link_libraries(hypervane-ui PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Network
    Qt6::Svg
    ${Protobuf_LIBRARIES}
    absl::hash
    absl::strings
    absl::log
    absl::log_internal_check_op
    absl::status
    absl::synchronization
)

# Assets path — override at configure time for installed builds (e.g. Nix).
if(NOT DEFINED HYPERVANE_ASSETS_DIR)
    set(HYPERVANE_ASSETS_DIR "${CMAKE_SOURCE_DIR}/../../assets")
endif()
target_compile_definitions(hypervane-ui PRIVATE
    HYPERVANE_ASSETS_DIR="${HYPERVANE_ASSETS_DIR}"
)

install(TARGETS hypervane-ui DESTINATION bin)
