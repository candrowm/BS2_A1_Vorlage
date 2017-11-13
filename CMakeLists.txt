project(chat_vorlage_1_)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -pthread")

set(SOURCE_FILES
        chat_server_threads.cpp
        chat_server_threads.h
        chat_server_poll.c
        chat_server_poll.h
        chat.c
        chat.h
        CMakeLists.txt
        error_reporting.c
        error_reporting.h
        software_information.h
        tcp_socket.c
        tcp_socket.h
        )

add_executable(chat_vorlage_1_ ${SOURCE_FILES})