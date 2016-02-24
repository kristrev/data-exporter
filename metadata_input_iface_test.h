#ifndef METADATA_INPUT_IFACE_TEST_H
#define METADATA_INPUT_IFACE_TEST_H


#define IFACE_REGISTER_TEST "{'timestamp':1455881186,'event_type':1,'iccid':'89480610500302602379'," \
                            "'imsi':'260060080260237','imei':'866948011255141','device_state':1," \
                            "'event_param':1, 'ifname':'usb0'}"

#define IFACE_UNREGISTER_TEST "{'timestamp':1455881286,'event_type':1,'iccid':'89480610500302602379'," \
                              "'imsi':'260060080260237','imei':'866948011255141','device_state':2," \
                              "'event_param':1, 'ifname':'usb0'}"

#define IFACE_CONNECT_TEST "{'timestamp':1455881186,'event_type':1,'iccid':'89480610500302602379'," \
                           "'imsi':'260060080260237','imei':'866948011255141','device_state':3," \
                           "'event_param':1, 'ifname':'usb0'}"

#define IFACE_DISCONNECT_TEST "{'timestamp':1455882186,'event_type':1,'iccid':'89480610500302602379'," \
                              "'imsi':'260060080260237','imei':'866948011255141','device_state':4," \
                              "'event_param':1, 'ifname':'usb0'}"

#define IFACE_MODE_CHANGED_TEST "{'timestamp':1453799160,'event_type':1,'iccid':'8948022214021296473'," \
                                "'imsi':'260021822129647','imei':'866948013972495','device_mode':5," \
                                "'device_sub_mode':0,'event_param':2, 'ifname':'usb0'}"

#define IFACE_SUBMODE_CHANGED_TEST "{'timestamp':1453799365,'event_type':1,'iccid':'8948022214021296473'," \
                                   "'imsi':'260021822129647','imei':'866948013972495','device_mode':4," \
                                   "'device_sub_mode':7,'event_param':2, 'ifname':'usb0'}"

#define IFACE_RSSI_CHANGED_TEST "{'timestamp':1455889019,'event_type':1,'iccid':'89480610500302602379'," \
                                "'imsi':'260060080260237','imei':'866948011255141','rssi':-78,'ecio':-10," \
                                "'rscp':-88,'event_param':3, 'ifname':'usb0'}"

#define IFACE_LTE_RSSI_CHANGED_TEST "{'timestamp':1455881893,'event_type':1,'iccid':'89480610500302602379'," \
                                    "'imsi':'260060080260237','imei':'866948011255141','lte_rssi':-73," \
                                    "'lte_rsrp':-96,'lte_rsrq':-8,'event_param':3, 'ifname':'usb0'}"

#define IFACE_LTE_BAND_CHANGED_TEST "{'timestamp':1453382715,'event_type':1,'iccid':'89480610500302602379'," \
                                    "'imsi':'260060080260237','imei':'866948013972495','lte_band':3," \
                                    "'lte_freq':1800,'event_param':4, 'ifname':'usb0'}"

#define IFACE_ISP_NAME_CHANGED_TEST "{'timestamp':1453821612,'event_type':1,'iccid':'89480610500302602319'," \
                                    "'imsi':'260021822129647','imei':'866948013172495','isp_name':'T-Mobile'," \
                                    "'event_param':5, 'ifname':'usb0'}"

#define IFACE_EXTERNAL_ADDR_CHANGED_TEST "{'timestamp':1455881186,'event_type':1,'iccid':'89480610500302602379'," \
                                         "'imsi':'260060080260237','imei':'866948011255141','ip_addr':'10.120.159.56'," \
                                         "'event_param':7, 'ifname':'usb0'}"

#define IFACE_LOCATION_CHANGED_TEST "{'timestamp':1455881213,'event_type':1,'iccid':'89480610500302602379'," \
                                    "'imsi':'260060080260237','imei':'866948011255141','lac':32,'cid':3262520," \
                                    "'lte_pci':13,'enodeb_id':3262520,'event_param':8, 'ifname':'usb0'}"

#define IFACE_NETWORK_MCC_CHANGED "{'timestamp':1456229848,'event_type':1,'iccid':'89480610500302602379'," \
                                  "'imsi':'260060080260237','imei':'861311010197670','network_mccmnc':26006," \
                                  "'event_param':9, 'ifname':'usb0'}"

#define IFACE_UPDATE_TEST "{'timestamp':1455881903,'event_type':1,'iccid':'89480610500302602379'," \
                          "'imsi':'260060080260237','imei':'866948011255141','imsi_mccmnc':26006," \
                          "'network_mccmnc':26006,'device_mode':5,'device_sub_mode':0,'isp_name':'Play'," \
                          "'lte_rssi':-69,'lte_rsrp':-96,'lte_rsrq':-11,'lac':32,'cid':3262520,'lte_pci':13," \
                          "'enodeb_id':3262520,'ip_addr':'10.120.159.56','internal_ip_addr':'192.168.32.113'," \
                          "'lte_band':3,'lte_freq':1800,'device_state':0,'event_param':6, 'ifname':'usb0'}"
#endif
