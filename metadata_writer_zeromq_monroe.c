#include "metadata_writer_zeromq.h"

const char *monroe_topics[MD_ZMQ_TOPICS_MAX + 1] = {
    "MONROE.META.NODE.EVENT",
    "MONROE.META.NODE.SENSOR",
    "MONROE.META.DEVICE.MODEM",
    "MONROE.META.DEVICE.CONNECTIVITY",
    "MONROE.META.DEVICE.GPS",
    "STATE",
    "MODE",
    "SIGNAL",
    "LTE_BAND",
    "ISP_NAME",
    "UPDATE",
    "IP_ADDR",
    "LOC_CHANGE",
    "NW_MCCMNC_CHANGE",
    "RADIO_CELL_LOCATION_GERAN",
    "RADIO_GSM_RR_CELL_SEL_RESEL_PARAM",
    "RADIO_GRR_CELL_RESEL",
    "RADIO_GSM_RR_CIPHER_MODE",
    "RADIO_GSM_RR_CHANNEL_CONF"
};

const char *monroe_keys[MD_ZMQ_KEYS_MAX + 1] = {
    "SequenceNumber",
    "Timestamp",
    "DataId",
    "DataVersion",
    "CID",
    "DeviceMode",
    "DeviceSubmode",
    "DeviceState",
    "ECIO",
    "ENODEBID",
    "ICCID",
    "InterfaceName",
    "IMSI",
    "IMSIMCCMNC",
    "IMEI",
    "IPAddress",
    "InternalIPAddress",
    "Operator",
    "LAC",
    "LTERSRP",
    "LTEFrequency",
    "LTERSSI",
    "LTERSRQ",
    "LTEBand",
    "LTEPCI",
    "Mode",
    "NWMCCMNC",
    "RSCP",
    "RSSI",
    "SignalStrength",
    "InterfaceId",
    "InterfaceName",
    "Operator",
    "Altitude",
    "Longitude",
    "Latitude",
    "NumberOfSatellites",
    "NMEA",
    "Speed",

    "CellID",
    "PLMN",
    "LAC",
    "ARFCN",
    "BSIC",
    "TimingAdvance",
    "RXLEV",
    "CellGeranInfoNumber",

    "ServingBCCHARFCN",
    "ServingPBCCHARFCN",
    "ServingPriorityClass",
    "ServingRXLEVAverage",
    "ServingC1",
    "ServingC2",
    "ServingC31",
    "ServingC32",
    "ServingFiveSecondTimer",
    "CellReselectStatus",
    "RecentCellSelection",
    "GRRCellNeighbors",

    "CipheringState",
    "CipheringAlgorithm",

    "CellReselectHysteresis",
    "MSTXPowerMaxCCH",
    "RXLEVAccessMin",
    "PowerOffsetValid",
    "PowerOffset",
    "NECI",
    "ACS",
    "OptReselectParamInd",
    "CellBarQualify",
    "CellReselectOffset",
    "TemporaryOffset",
    "PenaltyTime",

    "DedicatedChannelCount",
    "DTXIndicator",
    "PowerLevel",
    "StartingTimeValid",
    "StartingTime",
    "CipherFlag",
    "CipherAlgorithm",
    "AfterChannelConfig",
    "BeforeChannelCondif",
    "ChannelMode1",
    "ChannelMode2",

    "InternalInterface"
};
