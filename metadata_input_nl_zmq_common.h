#ifndef METADATA_INPUT_NL_ZMQ_COMMON
#define METADATA_INPUT_NL_ZMQ_COMMON

#include "metadata_exporter.h"

uint8_t parse_conn_info(struct json_object *meta_obj, struct md_conn_event *mce, struct md_exporter *parent);
uint8_t parse_iface_event(struct json_object *meta_obj, struct md_iface_event *mie, struct md_exporter *parent);
struct md_radio_cell_loc_geran_event* radio_cell_loc_geran(json_object *obj);
struct md_radio_grr_cell_resel_event* radio_grr_cell_resel(json_object *obj);
struct md_radio_gsm_rr_cell_sel_reset_param_event* radio_gsm_rr_cell_sel_reset_param(json_object *obj);
struct md_radio_gsm_rr_cipher_mode_event* radio_gsm_rr_cipher_mode(json_object *obj);
struct md_radio_gsm_rr_channel_conf_event* radio_gsm_rr_channel_conf(json_object *obj);
struct md_radio_wcdma_rrc_state_event* radio_wcdma_rrc_state(json_object *obj);
struct md_radio_wcdma_cell_id_event* radio_wcdma_cell_id(json_object *obj);
uint8_t add_json_key_value(const char *key, int32_t value, struct json_object *obj);
void init_iface_event(struct md_iface_event *mie);
void init_conn_event(struct md_conn_event *mce);
#endif
