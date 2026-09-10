cmake_minimum_required(VERSION 3.16)
project(BitVoid VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

# Core library (all sources except main.cpp) — shared by bitvoid-core and tests.
add_library(bitvoid-core-lib STATIC
    src/sha256.cpp
    src/block.cpp
    src/transaction.cpp
    src/mempool.cpp
    src/blockchain.cpp
    src/consensus.cpp
    src/wallet.cpp
    src/node.cpp
)

target_include_directories(bitvoid-core-lib PUBLIC src)

target_link_libraries(bitvoid-core-lib PUBLIC
    OpenSSL::Crypto
    OpenSSL::SSL
    Threads::Threads
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(bitvoid-core-lib PUBLIC ws2_32)
endif()

# Main executable.
add_executable(bitvoid-core src/main.cpp)
target_link_libraries(bitvoid-core PRIVATE bitvoid-core-lib)

install(TARGETS bitvoid-core RUNTIME DESTINATION bin)

# --- Tests ---
option(BUILD_TESTS "Build unit tests" ON)

if(BUILD_TESTS)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    enable_testing()
    include(GoogleTest)

    add_executable(bitvoid-tests
        tests/test_block.cpp
        tests/test_transaction.cpp
        tests/test_blockchain.cpp
        tests/test_wallet.cpp
        tests/test_consensus.cpp
        tests/test_mempool.cpp
    )

    target_link_libraries(bitvoid-tests PRIVATE
        bitvoid-core-lib
        GTest::gtest
        GTest::gtest_main
    )

    gtest_discover_tests(bitvoid-tests)
endif()
