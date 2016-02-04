#pragma once

#define MONROE_ZMQ_DATA_VERSION 1
#define MONROE_ZMQ_DATA_ID_SYSEVENT "MONROE.META.NODE.EVENT"
#define MONROE_ZMQ_DATA_ID_SENSOR   "MONROE.META.NODE.SENSOR"
#define MONROE_ZMQ_DATA_ID_MODEM    "MONROE.META.DEVICE.MODEM"
#define MONROE_ZMQ_DATA_ID_GPS      "MONROE.META.DEVICE.GPS"

#define ZMQ_KEY_SEQ           "seq"
#define ZMQ_KEY_TSTAMP        "tstamp"

## these are disabled if -DMONROE is not set
#define ZMQ_KEY_DATAID        "" 
#define ZMQ_KEY_DATAVERSION   ""

#define ZMQ_KEY_INTERFACEID   "if_id"
#define ZMQ_KEY_INTERFACENAME "if_name"
#define ZMQ_KEY_OPERATOR      "operator"
#define ZMQ_KEY_MODE          "mode"
#define ZMQ_KEY_SIGNAL        "signal"

#define ZMQ_KEY_ALTITUDE      "alt"
#define ZMQ_KEY_LONGITUDE     "lon"
#define ZMQ_KEY_LATITUDE      "lat"
#define ZMQ_KEY_NUMSAT        "num_sat"
#define ZMQ_KEY_NMEA          "nmea_raw"
#define ZMQ_KEY_SPEED         "speed"

