#include "user_key.h"

#define KEY_SHORT_PRESS_MINTIME		3
#define	KEY_LONG_PRESS_TIME_MIN		20
#define	KEY_LONG_PRESS_TIME			40
#define KEY_CONT_TIME				1

static const char *TAG = "KEY";

ESPFUNC key_para_t* user_key_init_single(uint8_t gpio_id, uint8_t gpio_func, uint32_t gpio_name,
													key_function_t short_press,
													key_function_t long_press,
													key_function_t cont_press,
													key_function_t release) {
	key_para_t *key = (key_para_t *)os_zalloc(sizeof(key_para_t));
	if (key != NULL) {
		key->rpt_count = 0;
		key->long_count = KEY_LONG_PRESS_TIME;
		key->gpio_id = gpio_id;
		key->gpio_func = gpio_func;
		key->gpio_name = gpio_name;
		key->short_press = short_press;
		key->long_press = long_press;
		key->cont_press = cont_press;
		key->release = release;
	}
	return key;
}

ESPFUNC void user_key_set_long_count(key_para_t *pkey, uint16_t count) {
	if (pkey != NULL) {
		if (count < KEY_LONG_PRESS_TIME_MIN) {
			pkey->long_count = KEY_LONG_PRESS_TIME_MIN;
		} else {
			pkey->long_count = count;
		}
	}
}

ESPFUNC static void user_key_20ms_cb(void *arg) {
	key_para_t *pkey = (key_para_t *) arg;
	if(pkey != NULL) {
		if (GPIO_INPUT_GET(GPIO_ID_PIN(pkey->gpio_id)) == 0) {
			pkey->rpt_count++;
			if (pkey->rpt_count == pkey->long_count) {
				if (pkey->long_press != NULL) {
					LOGD(TAG, "key long press.");
					pkey->long_press();
				}
			} else if (pkey->rpt_count >= pkey->long_count + KEY_CONT_TIME) {
				pkey->rpt_count = pkey->long_count;
				if (pkey->cont_press != NULL) {
					pkey->cont_press();
				}
			}
		} else {
			os_timer_disarm(&pkey->tmr_20ms);
			//clear gpio status
			GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(pkey->gpio_id));
			//enable interrupt
			gpio_pin_intr_state_set(GPIO_ID_PIN(pkey->gpio_id), GPIO_PIN_INTR_NEGEDGE);
			if (pkey->rpt_count >= pkey->long_count) {
				if (pkey->release != NULL) {
					LOGD(TAG, "key release.");
					pkey->release();
				}
			} else if (pkey->rpt_count >= KEY_SHORT_PRESS_MINTIME) {
				if (pkey->short_press != NULL) {
					LOGD(TAG, "key press short.");
					pkey->short_press();
				}
			}
			pkey->rpt_count = 0;
		}
	}
}

static void user_key_intr_handler(void *arg) {
	uint8_t i;
	key_list_t *plist = (key_list_t *) arg;
	if(plist != NULL) {
		uint32_t gpio_status = GPIO_REG_READ(GPIO_STATUS_ADDRESS);
		for (i = 0; i < plist->key_num; i++) {
			if (gpio_status & BIT(plist->pkeys[i]->gpio_id)) {
				//disable interrupt
				gpio_pin_intr_state_set(GPIO_ID_PIN(plist->pkeys[i]->gpio_id), GPIO_PIN_INTR_DISABLE);
				//clear interrupt status
				GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, gpio_status & BIT(plist->pkeys[i]->gpio_id));
				os_timer_disarm(&plist->pkeys[i]->tmr_20ms);
				os_timer_setfn(&plist->pkeys[i]->tmr_20ms, user_key_20ms_cb, plist->pkeys[i]);
				os_timer_arm(&plist->pkeys[i]->tmr_20ms, 20, 1);
			}
		}
	}
}

ESPFUNC void user_key_init_list(key_list_t *plist) {
	uint8_t i;
	if (plist == NULL) {
		return;
	}
	ETS_GPIO_INTR_ATTACH(user_key_intr_handler, plist);
	ETS_GPIO_INTR_DISABLE();
	for (i = 0; i < plist->key_num; i++) {
		PIN_FUNC_SELECT(plist->pkeys[i]->gpio_name, plist->pkeys[i]->gpio_func);
		GPIO_DIS_OUTPUT(plist->pkeys[i]->gpio_id);
		// gpio_output_set(0, 0, 0, 1<<GPIO_ID_PIN(plist->pkeys[i]->gpio_id));
		gpio_register_set(GPIO_PIN_ADDR(plist->pkeys[i]->gpio_id), GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
						  | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
						  | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
		//clear gpio status
		GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, BIT(plist->pkeys[i]->gpio_id));
		//enable interrupt
		gpio_pin_intr_state_set(GPIO_ID_PIN(plist->pkeys[i]->gpio_id), GPIO_PIN_INTR_NEGEDGE);
	}
	ETS_GPIO_INTR_ENABLE();
}
