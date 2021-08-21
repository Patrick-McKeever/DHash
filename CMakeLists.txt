cmake_minimum_required(VERSION 3.19)
project(chord_final)

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
)
FetchContent_Declare(
        JsonCpp
        GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
)
FetchContent_MakeAvailable(googletest)
FetchContent_MakeAvailable(JsonCpp)

add_executable(
        chord_final
        main.cpp
        src/finger_table.h
        src/peer_repr.cpp
        src/peer_repr.h
        src/peer.cpp
        src/peer.h
        src/server.h
        src/client.cpp
        src/client.h
        test/server_test.cc
        test/key_test.cc
        test/peer_test.cc
        src/key.cpp src/key.h src/data_block.h
        test/information_dispersal_test.cc src/merkle_node.cpp src/merkle_node.h src/data_block.cpp
        test/merkel_tree_test.cc src/database.cpp src/database.h src/finger_table.cpp)

find_package( Boost 1.40 COMPONENTS program_options REQUIRED )
find_package(Threads)
include_directories( ${Boost_INCLUDE_DIR} )

target_link_libraries( chord_final ${Boost_LIBRARIES} )

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

target_link_libraries(
        chord_final
        gtest_main
)
target_link_libraries(
        chord_final
        jsoncpp_lib
)