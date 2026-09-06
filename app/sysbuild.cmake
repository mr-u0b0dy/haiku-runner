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
  #
  # This sample file was renamed upstream (nrf5340_cpunet_iso_peripheral- ->
  # extra-iso_peripheral-) shortly after the v4.4.2 this project pins, so
  # check both names rather than assume: CI's workspace is pinned exactly to
  # v4.4.2, but a shared local west workspace (e.g. one also used by other
  # projects) may float ahead of that pin.
  set(HR_NET_CORE_ISO_PERIPHERAL_CONF_OLD
    ${NET_APP_SRC_DIR}/nrf5340_cpunet_iso_peripheral-bt_ll_sw_split.conf)
  set(HR_NET_CORE_ISO_PERIPHERAL_CONF_NEW
    ${NET_APP_SRC_DIR}/extra-iso_peripheral-bt_ll_sw_split.conf)

  if(EXISTS ${HR_NET_CORE_ISO_PERIPHERAL_CONF_NEW})
    set(HR_NET_CORE_ISO_PERIPHERAL_CONF ${HR_NET_CORE_ISO_PERIPHERAL_CONF_NEW})
  elseif(EXISTS ${HR_NET_CORE_ISO_PERIPHERAL_CONF_OLD})
    set(HR_NET_CORE_ISO_PERIPHERAL_CONF ${HR_NET_CORE_ISO_PERIPHERAL_CONF_OLD})
  else()
    message(FATAL_ERROR
      "Neither ${HR_NET_CORE_ISO_PERIPHERAL_CONF_NEW} nor "
      "${HR_NET_CORE_ISO_PERIPHERAL_CONF_OLD} exists - hci_ipc's ISO "
      "peripheral config sample may have moved again.")
  endif()

  set(${NET_APP}_EXTRA_CONF_FILE
    ${HR_NET_CORE_ISO_PERIPHERAL_CONF}
    CACHE INTERNAL ""
  )
endif()
