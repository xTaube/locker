file(GLOB LOCKER_SOURCES "locker/*.c")
add_executable(
    locker
    ${LOCKER_SOURCES}
)
target_include_directories(locker PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_compile_features(locker PRIVATE c_std_11)

set(CMAKE_CONFIGURATION_TYPES Debugger Development Release CACHE STRING "" FORCE)

target_compile_options(locker PRIVATE
    $<$<CONFIG:Debugger>:-O0 -g>
    $<$<CONFIG:Development>:-O1 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined>
    $<$<CONFIG:Release>:-O3 -w>
)
target_link_options(locker PRIVATE
    $<$<CONFIG:Development>:-fsanitize=address,undefined>
)

include(ExternalProject)

add_library(sqlite3 STATIC
    ${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.c
)

target_include_directories(sqlite3
    PUBLIC SYSTEM
        ${CMAKE_SOURCE_DIR}/third_party/sqlite
)
target_compile_options(sqlite3 PRIVATE -w)
target_compile_definitions(sqlite3
    PRIVATE SQLITE_THREADSAFE=1
)

add_library(SQLite::SQLite3 ALIAS sqlite3)

set(LIBSODIUM_INSTALL_DIR ${CMAKE_BINARY_DIR}/libsodium_install)

ExternalProject_Add(
    libsodium_ext
    SOURCE_DIR ${CMAKE_BINARY_DIR}/libsodium-src
    GIT_REPOSITORY https://github.com/jedisct1/libsodium.git
    GIT_TAG 1.0.19
    BUILD_IN_SOURCE 1

    CONFIGURE_COMMAND ./configure --prefix=${LIBSODIUM_INSTALL_DIR}
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)
add_library(sodium INTERFACE)
add_library(Libsodium::sodium ALIAS sodium)

target_include_directories(sodium INTERFACE SYSTEM ${LIBSODIUM_INSTALL_DIR}/include)

add_library(sodium_lib STATIC IMPORTED GLOBAL)
set_target_properties(sodium_lib PROPERTIES IMPORTED_LOCATION ${LIBSODIUM_INSTALL_DIR}/lib/libsodium.a)

add_dependencies(sodium_lib libsodium_ext)
add_dependencies(locker sodium_lib)

set(NCURSES_INSTALL_DIR ${CMAKE_BINARY_DIR}/ncurses-install)

ExternalProject_Add(
    ncurses_ext
    URL https://ftp.gnu.org/gnu/ncurses/ncurses-6.5.tar.gz
    BUILD_IN_SOURCE 1

    CONFIGURE_COMMAND
        ./configure
        --prefix=${NCURSES_INSTALL_DIR}
        --with-termlib
        --without-debug
    BUILD_COMMAND make
    INSTALL_COMMAND make install
)
add_library(ncurses INTERFACE)
add_library(Ncurses::Ncurses ALIAS ncurses)

target_include_directories(ncurses INTERFACE SYSTEM ${NCURSES_INSTALL_DIR}/include)

add_library(ncursesw STATIC IMPORTED GLOBAL)
set_target_properties(ncursesw PROPERTIES IMPORTED_LOCATION ${NCURSES_INSTALL_DIR}/lib/libncursesw.a)
add_dependencies(ncursesw ncurses_ext)

add_library(tinfow STATIC IMPORTED GLOBAL)
set_target_properties(tinfow PROPERTIES IMPORTED_LOCATION ${NCURSES_INSTALL_DIR}/lib/libtinfow.a)
add_dependencies(tinfow ncurses_ext)

add_dependencies(locker ncursesw)
add_dependencies(locker tinfow)

target_link_libraries(ncurses INTERFACE ncursesw tinfow sodium_lib)

target_link_libraries(locker PRIVATE SQLite::SQLite3 Libsodium::sodium Ncurses::Ncurses)

install(TARGETS locker RUNTIME DESTINATION bin)
