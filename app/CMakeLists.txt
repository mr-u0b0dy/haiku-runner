cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(haiku_runner)

target_include_directories(app PRIVATE include)

target_sources(app PRIVATE
  src/main.c
  src/audio/audio_router.c
  src/audio/audio_pipeline_zephyr.c
  src/input/input_frame_ingress.c
  src/input/input_registry.c
  src/input/source_manager.c
  src/app_state.c
)

if(CONFIG_HR_BACKEND_I2S)
  target_sources(app PRIVATE src/audio/audio_backend_i2s.c)
else()
  target_sources(app PRIVATE src/audio/audio_backend_stub.c)
endif()

if(CONFIG_HR_PIPELINE_SOF)
  target_sources(app PRIVATE src/audio/audio_pipeline_sof_adapter.c)
endif()

if(CONFIG_HR_INPUT_BLE)
  target_sources(app PRIVATE
    src/ble/le_audio_sink.c
    src/ble/le_audio_events.c
  )
endif()

if(CONFIG_HR_INPUT_AUX)
  target_sources(app PRIVATE src/input/input_aux_jack.c)
endif()

if(CONFIG_HR_INPUT_USB_C)
  target_sources(app PRIVATE src/input/input_usb_c_uac.c)
  if(CONFIG_HR_INPUT_USB_C_MOCK_FEEDER)
    target_sources(app PRIVATE src/input/input_usb_c_mock_feeder.c)
  endif()
endif()

if(CONFIG_HR_INPUT_WIFI)
  target_sources(app PRIVATE src/input/input_wifi_stream.c)
endif()
