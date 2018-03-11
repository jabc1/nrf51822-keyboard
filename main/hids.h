#ifndef __HIDS_H__
#define __HIDS_H__

#include "ble.h"
#include <stdint.h>
#include <stdbool.h>

void hids_init(void);
void hids_on_ble_evt(ble_evt_t *p_ble_evt);

void hids_buffer_init(void);
uint32_t hids_buffer_dequeue(bool tx_flag);
void hids_keys_send(uint8_t key_pattern_len, uint8_t *p_key_pattern);



#endif