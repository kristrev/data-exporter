#Standard stuff
cmake_minimum_required(VERSION 2.6)
project(data_exporter)

set(CMAKE_C_FLAGS "-O1 -Wall")
#Make sqlite and ZMQ conditional
set(LIBS pthread mnl json-c)
set(SOURCE
    metadata_exporter.c
    netlink_helpers.c
    backend_event_loop.c
    metadata_utils.c
    system_helpers.c
    metadata_input_netlink.c
    lib/minmea.c)
            

if (TARGET_OWRT)
    MESSAGE(STATUS "OpenWRT build, will include UCI")
    add_definitions("-DOPENWRT")
    LIST(APPEND LIBS uci)
else()
    MESSAGE(STATUS "Standard build")
    add_definitions("-DUBUNTU_MONROE")
endif()

if (SQLITE3)
    set(LIBS ${LIBS} sqlite3)
    set(SOURCE ${SOURCE} 
        metadata_writer_sqlite.c
        metadata_writer_sqlite_helpers.c
        metadata_writer_sqlite_conn.c
        metadata_writer_sqlite_gps.c)
    add_definitions("-DSQLITE_SUPPORT")
endif()

if (ZEROMQ)
    set(LIBS ${LIBS} zmq)
    set(SOURCE ${SOURCE}
        metadata_writer_zeromq.c)
    add_definitions("-DZEROMQ_SUPPORT")
endif()

if (GPSD)
    set(LIBS ${LIBS} gps)
    set(SOURCE ${SOURCE}
        metadata_input_gpsd.c)
    add_definitions("-DGPSD_SUPPORT")
endif()

if (GPS_NSB)
    set(SOURCE ${SOURCE}
        metadata_input_gps_nsb.c)
    add_definitions("-DNSB_GPS")
endif()

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION_MAJOR "0")
set(CPACK_PACKAGE_VERSION_MINOR "1")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Kristian R. Evensen <kristian.evensen@gmail.com>")
set (CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)

set(CMAKE_C_LINK_EXECUTABLE "${CMAKE_C_LINK_EXECUTABLE} -s")
add_executable(data_exporter ${SOURCE})
target_link_libraries(data_exporter ${LIBS})


if (TARGET_OWRT)
    install(TARGETS data_exporter RUNTIME DESTINATION bin)
else()
    install(TARGETS data_exporter RUNTIME DESTINATION /usr/local/sbin)
endif()
