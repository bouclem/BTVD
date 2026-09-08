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

add_executable(bitvoid-core
    src/sha256.cpp
    src/block.cpp
    src/transaction.cpp
    src/mempool.cpp
    src/blockchain.cpp
    src/consensus.cpp
    src/wallet.cpp
    src/node.cpp
    src/main.cpp
)

target_include_directories(bitvoid-core PRIVATE src)

target_link_libraries(bitvoid-core PRIVATE
    OpenSSL::Crypto
    OpenSSL::SSL
    Threads::Threads
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(bitvoid-core PRIVATE ws2_32)
endif()

install(TARGETS bitvoid-core RUNTIME DESTINATION bin)
