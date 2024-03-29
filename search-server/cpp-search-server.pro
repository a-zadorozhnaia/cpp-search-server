TEMPLATE = app
CONFIG += console c++17
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        document.cpp \
        main.cpp \
        process_queries.cpp \
        read_input_functions.cpp \
        remove_duplicates.cpp \
        request_queue.cpp \
        search_server.cpp \
        string_processing.cpp \
        test_search_server.cpp

HEADERS += \
    document.h \
    log_duration.h \
    paginator.h \
    print_out.h \
    process_queries.h \
    read_input_functions.h \
    remove_duplicates.h \
    request_queue.h \
    search_server.h \
    string_processing.h \
    test_search_server.h \
    concurrent_map.h \
    print_out.h
