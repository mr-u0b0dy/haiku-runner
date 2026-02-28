#ifndef HAIKU_RUNNER_INPUT_ADAPTERS_H_
#define HAIKU_RUNNER_INPUT_ADAPTERS_H_

int input_adapter_ble_register(void);
int input_adapter_aux_register(void);
int input_adapter_usb_c_register(void);
int input_adapter_wifi_register(void);

#endif
