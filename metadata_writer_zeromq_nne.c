#include "metadata_writer_zeromq.h"

const char *nne_topics[MD_ZMQ_TOPICS_MAX + 1] = {
    "NNE.META.NODE.EVENT",
    "NNE.META.NODE.SENSOR",
    "NNE.META.DEVICE.MODEM",
    "NNE.META.DEVICE.CONNECTIVITY",
    "NNE.META.DEVICE.GPS",
    "STATE",
    "MODE",
    "SIGNAL",
    "LTE_BAND",
    "ISP_NAME",
    "UPDATE",
    "IP_ADDR",
    "LOC_CHANGE",
    "NW_MCCMNC_CHANGE",
    "RADIO.CELL_LOCATION_GERAN",
    "RADIO.GSM_RR_CELL_SEL_RESEL_PARAM",
    "RADIO.GRR_CELL_RESEL",
    "RADIO.GSM_RR_CIPHER_MODE",
    "RADIO.GSM_RR_CHANNEL_CONF",
    "RADIO.WCDMA_RRC_STATE",
    "RADIO.WCDMA_CELL_ID"
};

const char *nne_keys[MD_ZMQ_KEYS_MAX + 1] = {
    "seq",
    "tstamp",
    NULL,
    "data_version",
    "cid",
    "device_mode",
    "device_submode",
    "device_state",
    "ecio",
    "enodeb_id",
    "iccid",
    "if_name",
    "imsi",
    "imsi_mccmnc",
    "imei",
    "ip_addr",
    "internal_ip_addr",
    "isp_name",
    "lac",
    "lte_rsrp",
    "lte_freq",
    "lte_rssi",
    "lte_rsrq",
    "lte_band",
    "lte_pci",
    "mode",
    "nw_mccmnc",
    "rscp",
    "rssi",
    "signal",
    "interface_id",
    "interface_name",
    "operator",
    "alt",
    "lon",
    "lat",
    "num_sat",
    "nmea_raw",
    "speed",

    "cell_id",
    "plmn",
    "lac",
    "arfcn",
    "bsic",
    "timing_advance",
    "rx_lev",
    "cell_geran_info_nmr",

    "serving_bcch_arfcn",
    "serving_pbcch_arfcn",
    "serving_priority_class",
    "serving_rxlev_avg",
    "serving_c1",
    "serving_c2",
    "serving_c31",
    "serving_c32",
    "serving_five_second_timer",
    "cell_reselect_status",
    "recent_cell_selection",
    "grr_cell_neighbors",

    "ciphering_state",
    "ciphering_algorithm",

    "cell_reselect_hysteresis",
    "ms_txpwr_max_cch",
    "rxlev_access_min",
    "power_offset_valid",
    "power_offset",
    "neci",
    "acs",
    "opt_reselect_param_ind",
    "cell_bar_qualify",
    "cell_reselect_offset",
    "temporary_offset",
    "penalty_time",

    "num_ded_chans",
    "dtx_indicator",
    "power_level",
    "starting_time_valid",
    "starting_time",
    "cipher_flag",
    "cipher_algorithm",
    "after_channel_config",
    "before_channel_condif",
    "channel_mode_1",
    "channel_mode_2",

    NULL
};
