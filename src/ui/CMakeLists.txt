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
)

find_package(Protobuf REQUIRED)
find_package(absl REQUIRED)

set(SOURCES
    main.cpp
    src/MainWindow.cpp
    src/LibraryView.cpp
    src/PreviewPlayer.cpp
    src/GameGridDelegate.cpp
    src/IPCClient.cpp
    src/IntroOverlay.cpp
)

set(HEADERS
    include/MainWindow.h
    include/LibraryView.h
    include/PreviewPlayer.h
    include/GameGridDelegate.h
    include/IPCClient.h
    include/IntroOverlay.h
)

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS
    ${CMAKE_SOURCE_DIR}/../../proto/hypervane.proto
)

qt_add_executable(hypervane-ui
    ${SOURCES}
    ${HEADERS}
    ${PROTO_SRCS}
    ${PROTO_HDRS}
    resources/resources.qrc
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
    ${Protobuf_LIBRARIES}
    absl::hash
    absl::strings
    absl::log
    absl::status
    absl::synchronization
)

# Bake the project assets path in so IntroOverlay can find the intro video.
target_compile_definitions(hypervane-ui PRIVATE
    HYPERVANE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/../../assets"
)

install(TARGETS hypervane-ui DESTINATION bin)
