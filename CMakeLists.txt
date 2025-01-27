# CMakeLists.txt
cmake_minimum_required(VERSION 3.10)
project(websocket_client)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Create library target
add_library(websocket_client_lib
    src/websocket_client.cpp
)

# Link library dependencies
target_link_libraries(websocket_client_lib
    PRIVATE
        Boost::system
        OpenSSL::SSL
        OpenSSL::Crypto
        pthread
)

# Create example executable
add_executable(websocket_client_example
    examples/main.cpp
)

# Link example with our library
target_link_libraries(websocket_client_example
    PRIVATE
        websocket_client_lib
)
