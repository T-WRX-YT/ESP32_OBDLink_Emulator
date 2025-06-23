#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <inttypes.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_bt_device.h"
#include "esp_spp_api.h"
extern void BTM_SetDefaultLinkPolicy(uint16_t settings);

#include "time.h"
#include "sys/time.h"

#define SPP_TAG "OBDLINK_EMULATOR"
#define SPP_SERVER_NAME "OBLINK_EMULATOR"
#define SPP_SHOW_DATA 0
#define SPP_SHOW_SPEED 1
#define SPP_SHOW_MODE SPP_SHOW_DATA    /*Choose show mode: show data or speed*/

#include <inttypes.h>

static const char local_device_name[] = CONFIG_EXAMPLE_LOCAL_DEVICE_NAME;
static const esp_spp_mode_t esp_spp_mode = ESP_SPP_MODE_CB;
static const bool esp_spp_enable_l2cap_ertm = true;

static struct timeval time_new, time_old;
static long data_num = 0;

static const esp_spp_sec_t sec_mask = ESP_SPP_SEC_AUTHENTICATE;
static const esp_spp_role_t role_slave = ESP_SPP_ROLE_SLAVE;

bool sendData = 0;
bool isConnected = 0;
int protocolMode = 0;
static uint32_t spp_client_handle = 0;

static char *bda2str(uint8_t * bda, char *str, size_t size)
{
    if (bda == NULL || str == NULL || size < 18) {
        return NULL;
    }

    uint8_t *p = bda;
    sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
            p[0], p[1], p[2], p[3], p[4], p[5]);
    return str;
}


// main "loop()"
void my_loop_task(void *arg) {
    while (1) {
        
        if (sendData && protocolMode == 1) {
            char bt_buf[128];
            int len = snprintf(bt_buf, sizeof(bt_buf), "%03X%02X%02X%02X%02X%02X%02X%02X%02X\r", 0x123, 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x02, 0x03, 0x04);

            esp_err_t err = esp_spp_write(spp_client_handle, len, (uint8_t *)bt_buf);
            if (err != ESP_OK) {
                ESP_LOGE(SPP_TAG, "Failed to send hex response: %s", esp_err_to_name(err));
            } else {
                ESP_LOGI(SPP_TAG, "Sent hex response");
            }
			
        }
		vTaskDelay(pdMS_TO_TICKS(100));
        
    }
}



static void print_speed(void)
{
    float time_old_s = time_old.tv_sec + time_old.tv_usec / 1000000.0;
    float time_new_s = time_new.tv_sec + time_new.tv_usec / 1000000.0;
    float time_interval = time_new_s - time_old_s;
    float speed = data_num * 8 / time_interval / 1000.0;
    ESP_LOGI(SPP_TAG, "speed(%fs ~ %fs): %f kbit/s" , time_old_s, time_new_s, speed);
    data_num = 0;
    time_old.tv_sec = time_new.tv_sec;
    time_old.tv_usec = time_new.tv_usec;
}

static void esp_spp_cb(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    char bda_str[18] = {0};

    switch (event) {
    case ESP_SPP_INIT_EVT:
        if (param->init.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "ESP_SPP_INIT_EVT");
            esp_spp_start_srv(sec_mask, role_slave, 0, SPP_SERVER_NAME);
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_INIT_EVT status:%d", param->init.status);
        }
        break;
    case ESP_SPP_DISCOVERY_COMP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_DISCOVERY_COMP_EVT");
        break;
    case ESP_SPP_OPEN_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_OPEN_EVT");
        break;
    case ESP_SPP_CLOSE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CLOSE_EVT status:%d handle:%"PRIu32" close_by_remote:%d", param->close.status,
                 param->close.handle, param->close.async);
        isConnected = 0;
		protocolMode = 0;
		sendData = 0;
        break;
    case ESP_SPP_START_EVT:
        if (param->start.status == ESP_SPP_SUCCESS) {
            ESP_LOGI(SPP_TAG, "ESP_SPP_START_EVT handle:%"PRIu32" sec_id:%d scn:%d", param->start.handle, param->start.sec_id,
                     param->start.scn);
            esp_bt_gap_set_device_name(local_device_name);
            esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);
        } else {
            ESP_LOGE(SPP_TAG, "ESP_SPP_START_EVT status:%d", param->start.status);
        }
        break;
    case ESP_SPP_CL_INIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CL_INIT_EVT");
        break;
    case ESP_SPP_DATA_IND_EVT:      /* RECEIVED DATA */
        #if (SPP_SHOW_MODE == SPP_SHOW_DATA)
        /*
         * We only show the data in which the data length is less than 128 here. If you want to print the data and
         * the data rate is high, it is strongly recommended to process them in other lower priority application task
         * rather than in this callback directly. Since the printing takes too much time, it may stuck the Bluetooth
         * stack and also have a effect on the throughput!
         */
        ESP_LOGI(SPP_TAG, "ESP_SPP_DATA_IND_EVT len:%d handle:%"PRIu32,
                 param->data_ind.len, param->data_ind.handle);
        if (param->data_ind.len < 128) {
            ESP_LOG_BUFFER_HEX("", param->data_ind.data, param->data_ind.len);
            ESP_LOGI(SPP_TAG, "Received data: %.*s", param->data_ind.len, (char *)param->data_ind.data);


            char buf[128];  // Adjust size as needed
            int len = param->data_ind.len > sizeof(buf) - 1 ? sizeof(buf) - 1 : param->data_ind.len;
            memcpy(buf, param->data_ind.data, len);
            buf[len] = '\0';

            while (len > 0 && (buf[len - 1] == '\r' || buf[len - 1] == '\n')) {
                buf[--len] = '\0';
            }

            ESP_LOGI(SPP_TAG, "Trimmed input: '%s'", buf);

            if ((strcmp(buf, "ATZ") == 0) || (strcmp(buf, "ATE0") == 0) || (strcmp(buf, "ATS0") == 0) || (strcmp(buf, "ATL0") == 0)) {
				ESP_LOGW(SPP_TAG, "Handshake message, sending OK");
				sendData = 0;
				const char *reply = "OK\r\r>";
				esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
			}
			else if (strcmp(buf, "STM") == 0) {
                ESP_LOGW(SPP_TAG, "Handshake complete");
                sendData = 1;
            }
            else if (strcmp(buf, "ATPC") == 0) {
                ESP_LOGW(SPP_TAG, "Monitor stop");
                protocolMode = 0;
				sendData = 0;
            }
			else if (strcmp(buf, "STP 33") == 0) {
				ESP_LOGW(SPP_TAG, "Canbus mode detected, sending OK");
				protocolMode = 1;
                const char *reply = "OK\r\r>";
                esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
			}
			else if (strcmp(buf, "ATSP00") == 0) {
				ESP_LOGW(SPP_TAG, "OBD2 mode detected, sending OK");
				protocolMode = 2;
                const char *reply = "OK\r\r>";
                esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
			}
			else if (strcmp(buf, "ATIGN") == 0) {
				ESP_LOGE(SPP_TAG, "ATIGN received.  Not sending a response.");
				//sendData = 0;
				//protocolMode = 0;
                //const char *reply = "WAKE\r\r>";
                //esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
			}
			else if ((strcmp(buf, "ATDP") == 0) && (protocolMode == 2)) {
				ESP_LOGE(SPP_TAG, "hit ATDP and protocolMode = 2, should start replying to obd requests.  Sending OK");
				sendData = 1;
                const char *reply = "OK\r\r>";
                esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
			}
			else if ((strcmp(buf, "010C") == 0) || (strcmp(buf, "0111") == 0) || (strcmp(buf, "010B") == 0) || (strcmp(buf, "010A") == 0) || (strcmp(buf, "0105") == 0) || (strcmp(buf, "015C") == 0) || (strcmp(buf, "010F") == 0)) {
				ESP_LOGW(SPP_TAG, "Received OBD2 command");
								
				if (strcmp(buf, "010C") == 0) {
					// Simulate 3000 RPM
					uint16_t rpm_raw = 3000 * 4; // 12000
					uint8_t A = (rpm_raw >> 8) & 0xFF;
					uint8_t B = rpm_raw & 0xFF;


					static char buffer[32];  // static ensures it has storage duration throughout
					sprintf(buffer, "41 0C %02X %02X\r>", A, B);
					const char *reply = buffer;
					esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
					ESP_LOGE(SPP_TAG, "Sent OBD response");
				}	
				
				if (strcmp(buf, "0111") == 0) {
					// Simulate 50% throttle
					uint8_t A = 0x7F;  // ~50% throttle
					static char buffer[16];
					sprintf(buffer, "41 11 %02X\r>", A);
					const char *reply = buffer;
					esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
					ESP_LOGE(SPP_TAG, "Sent OBD response");
				}
				if (strcmp(buf, "010B") == 0) {
					uint8_t A = 40;  // 40 kPa
					char buffer[16];
					sprintf(buffer, "41 0B %02X\r>", A);
					const char *reply = buffer;
					esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
					ESP_LOGE(SPP_TAG, "Sent OBD response");
				}
				if (strcmp(buf, "010A") == 0) {
					uint8_t A = 100;  // 100 × 3 = 300 kPa
					char buffer[16];
					sprintf(buffer, "41 0A %02X\r>", A);
					const char *reply = buffer;
					esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
					ESP_LOGE(SPP_TAG, "Sent OBD response");
				}
				if (strcmp(buf, "0105") == 0) {
					uint8_t A = 90 + 40;  // 130 = 0x82
					char buffer[16];
					sprintf(buffer, "41 05 %02X\r>", A);
					const char *reply = buffer;
					esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
					ESP_LOGE(SPP_TAG, "Sent OBD response");
				}
				if (strcmp(buf, "015C") == 0) {
					uint8_t A = 100 + 40;  // 140 = 0x8C
					char buffer[16];
					sprintf(buffer, "41 5C %02X\r>", A);
					const char *reply = buffer;
					esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
					ESP_LOGE(SPP_TAG, "Sent OBD response");
				}
				if (strcmp(buf, "010F") == 0) {
					uint8_t A = 35 + 40;  // 75 = 0x4B
					char buffer[16];
					sprintf(buffer, "41 0F %02X\r>", A);
					const char *reply = buffer;
					esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
					ESP_LOGE(SPP_TAG, "Sent OBD response");
				}				
			}			
            else {
                ESP_LOGW(SPP_TAG, "Unparsed Message received, sending OK");
                sendData = 0;
                const char *reply = "OK\r\r>";
                esp_spp_write(param->data_ind.handle, strlen(reply), (uint8_t *)reply);
            }	
        }
#else
        gettimeofday(&time_new, NULL);
        data_num += param->data_ind.len;
        if (time_new.tv_sec - time_old.tv_sec >= 3) {
            print_speed();
        }
#endif
        break;
    case ESP_SPP_CONG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_CONG_EVT");
        break;
    case ESP_SPP_WRITE_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_WRITE_EVT");
        break;
    case ESP_SPP_SRV_OPEN_EVT:
        spp_client_handle = param->srv_open.handle;
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_OPEN_EVT status:%d handle:%"PRIu32", rem_bda:[%s]", param->srv_open.status,
                 param->srv_open.handle, bda2str(param->srv_open.rem_bda, bda_str, sizeof(bda_str)));
        gettimeofday(&time_old, NULL);
        isConnected = 1;



        break;
    case ESP_SPP_SRV_STOP_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_SRV_STOP_EVT");
        break;
    case ESP_SPP_UNINIT_EVT:
        ESP_LOGI(SPP_TAG, "ESP_SPP_UNINIT_EVT");
        break;
    default:
        break;
    }
}

void esp_bt_gap_cb(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    char bda_str[18] = {0};

    switch (event) {
    case ESP_BT_GAP_AUTH_CMPL_EVT:{
        if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
            ESP_LOGI(SPP_TAG, "authentication success: %s bda:[%s]", param->auth_cmpl.device_name,
                     bda2str(param->auth_cmpl.bda, bda_str, sizeof(bda_str)));
        } else {
            ESP_LOGE(SPP_TAG, "authentication failed, status:%d", param->auth_cmpl.stat);
        }
        break;
    }
    case ESP_BT_GAP_PIN_REQ_EVT:{
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_PIN_REQ_EVT min_16_digit:%d", param->pin_req.min_16_digit);
        if (param->pin_req.min_16_digit) {
            ESP_LOGI(SPP_TAG, "Input pin code: 0000 0000 0000 0000");
            esp_bt_pin_code_t pin_code = {0};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
        } else {
            ESP_LOGI(SPP_TAG, "Input pin code: 1234");
            esp_bt_pin_code_t pin_code;
            pin_code[0] = '1';
            pin_code[1] = '2';
            pin_code[2] = '3';
            pin_code[3] = '4';
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
        }
        break;
    }

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
    case ESP_BT_GAP_CFM_REQ_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_CFM_REQ_EVT Please compare the numeric value: %06"PRIu32, param->cfm_req.num_val);
        esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
        break;
    case ESP_BT_GAP_KEY_NOTIF_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_NOTIF_EVT passkey:%06"PRIu32, param->key_notif.passkey);
        break;
    case ESP_BT_GAP_KEY_REQ_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_KEY_REQ_EVT Please enter passkey!");
        break;
#endif

    case ESP_BT_GAP_MODE_CHG_EVT:
        ESP_LOGI(SPP_TAG, "ESP_BT_GAP_MODE_CHG_EVT mode:%d bda:[%s]", param->mode_chg.mode,
                 bda2str(param->mode_chg.bda, bda_str, sizeof(bda_str)));
        break;

    default: {
        ESP_LOGI(SPP_TAG, "event: %d", event);
        break;
    }
    }
    return;
}

void app_main(void)
{
    char bda_str[18] = {0};
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_BLE));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if ((ret = esp_bt_controller_init(&bt_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
#if (CONFIG_EXAMPLE_SSP_ENABLED == false)
    bluedroid_cfg.ssp_en = false;
#endif
    if ((ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s initialize bluedroid failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    ESP_LOGE(SPP_TAG, "Enabled bluedroid");
    #define HCI_ENABLE_MASTER_SLAVE_SWITCH 0x0001
    BTM_SetDefaultLinkPolicy(HCI_ENABLE_MASTER_SLAVE_SWITCH);
    ESP_LOGE(SPP_TAG, "Sniff mode disabled via BTM_SetDefaultLinkPolicy");

    //if ((ret = esp_bluedroid_enable()) != ESP_OK) {
    //    ESP_LOGE(SPP_TAG, "%s enable bluedroid failed: %s", __func__, esp_err_to_name(ret));
    //    return;
    //}

    if ((ret = esp_bt_gap_register_callback(esp_bt_gap_cb)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s gap register failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    if ((ret = esp_spp_register_callback(esp_spp_cb)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp register failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    esp_spp_cfg_t bt_spp_cfg = {
        .mode = esp_spp_mode,
        .enable_l2cap_ertm = esp_spp_enable_l2cap_ertm,
        .tx_buffer_size = 0, /* Only used for ESP_SPP_MODE_VFS mode */
    };
    if ((ret = esp_spp_enhanced_init(&bt_spp_cfg)) != ESP_OK) {
        ESP_LOGE(SPP_TAG, "%s spp init failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

#if (CONFIG_EXAMPLE_SSP_ENABLED == true)
    /* Set default parameters for Secure Simple Pairing */
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_IO;
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));
#endif

    /*
     * Set default parameters for Legacy Pairing
     * Use variable pin, input pin code when pairing
     */
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_VARIABLE;
    esp_bt_pin_code_t pin_code;
    esp_bt_gap_set_pin(pin_type, 0, pin_code);

    ESP_LOGI(SPP_TAG, "Own address:[%s]", bda2str((uint8_t *)esp_bt_dev_get_address(), bda_str, sizeof(bda_str)));



    xTaskCreate(my_loop_task, "loop_task", 2048, NULL, 5, NULL);


}