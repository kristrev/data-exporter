#ifndef METADATA_INPUT_IFACE_TEST_H
#define METADATA_INPUT_IFACE_TEST_H

#define IFACE_REGISTER_TEST "{'timestamp':1453739333,'event_type':1,'iccid':'8948022214021296473'," \
                            "'imsi':'260021822129647','imei':'866948013972495','device_state':1," \
                            "'event_param':1}"

#define IFACE_UNREGISTER_TEST "{'timestamp':1453799141,'event_type':1,'iccid':'8948022214021296473'," \
                              "'imsi':'260021822129647','imei':'866948013972495','device_state':2,'" \
                              "event_param':1}"

#define IFACE_CONNECT_TEST "{'timestamp':1463579179,'event_type':1,'iccid':'89480610500302602379'," \
                           "'imsi':'260060080260237','imei':'866948013972495','device_state':3," \
                           "'event_param':2}"

#define IFACE_DISCONNECT_TEST "{'timestamp':1463579179,'event_type':1,'iccid':'89480610500302602379'," \
                           "'imsi':'260060080260237','imei':'866948013972495','device_state':4," \
                           "'event_param':2}"

#define IFACE_MODE_CHANGED_TEST "{'timestamp':1453799160,'event_type':1,'iccid':'8948022214021296473'," \
                           "'imsi':'260021822129647','imei':'866948013972495','device_mode':4," \
                           "'device_sub_mode':1,'event_param':3}"

#define IFACE_SUBMODE_CHANGED_TEST "{'timestamp':1453799160,'event_type':1,'iccid':'8948022214021296473'," \
                              "'imsi':'260021822129647','imei':'866948013972495','device_mode':4," \
                              "'device_sub_mode':7,'event_param':3}"

#define IFACE_RSSI_CHANGED_TEST "{'timestamp':1453382564,'event_type':1,'iccid':'89480610500302602379'," \
                           "'imsi':'260060080260237','imei':'866948013972495','rssi':-75," \
                           "'event_param':4}"

#define IFACE_LTE_RSSI_CHANGED_TEST "{'timestamp':1453382564,'event_type':1,'iccid':'89480610500302602379'," \
                               "'imsi':'260060080260237','imei':'866948013972495','lte_rssi':-75," \
                               "'event_param':4}"

#define IFACE_LTE_BAND_CHANGED_TEST "{'timestamp':1453382715,'event_type':1,'iccid':'89480610500302602379'," \
                               "'imsi':'260060080260237','imei':'866948013972495','lte_band':3," \
                               "'lte_freq':1800,'event_param':5}"

#define IFACE_UPDATE_TEST "{'timestamp':1453377359,'event_type':1,'iccid':'89480610500302602379'," \
                        "'imsi':'260060080260237','imei':'866948013972495','imsi_mccmnc':26006," \
                        "'network_mccmnc':26006,'device_mode':5,'device_sub_mode':0,'isp_name':'Play P4',"\
                        "'rssi':-70,'rsrp':-97,'rsrq':-11,'lac':'20','cid':'31c838','lte_band':3," \
                        "'lte_freq':1800,'device_state':3,'event_param':6}"

#endif
