# Builds the nRF5340 network-core Bluetooth controller image (hci_ipc) and
# bundles it alongside the application-core image, so `west build --sysbuild`
# produces a flashable dual-core BLE Audio speaker.
if(SB_CONFIG_NET_CORE_IMAGE_HCI_IPC)
  set(NET_APP hci_ipc)
  set(NET_APP_SRC_DIR ${ZEPHYR_BASE}/samples/bluetooth/${NET_APP})

  ExternalZephyrProject_Add(
    APPLICATION ${NET_APP}
    SOURCE_DIR  ${NET_APP_SRC_DIR}
    BOARD       ${SB_CONFIG_NET_CORE_BOARD}
  )

  # Peripheral-only ISO controller config: matches our role exactly (a BLE
  # Audio unicast sink only ever accepts a connection and receives CIS
  # audio; it never scans, centrals, or broadcasts).
  set(${NET_APP}_EXTRA_CONF_FILE
    ${NET_APP_SRC_DIR}/extra-iso_peripheral-bt_ll_sw_split.conf
    CACHE INTERNAL ""
  )
endif()
