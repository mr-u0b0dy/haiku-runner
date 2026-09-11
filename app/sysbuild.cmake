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

# The hfclkaudio devicetree node (and the clock_control_nrf_hfclkaudio driver
# that reads it) arrived in Zephyr shortly after v4.4.2, which this project
# pins for CI. A pinned v4.4.2 checkout has no such label - referencing it
# unconditionally in the app overlay is a devicetree parse error there - but
# a newer Zephyr's split clock_control_nrf drivers select
# CLOCK_CONTROL_NRF_HFCLKAUDIO, which needs that node's hfclkaudio-frequency
# property instead of the legacy one on &clock the main overlay sets, and
# I2S's ACLK clock source BUILD_ASSERT fails there without it. Detect which
# devicetree this checkout actually has and layer on the extra overlay only
# when the node exists, so both a CI workspace pinned exactly to v4.4.2 and a
# local checkout that floats ahead of that pin (e.g. one shared with other
# projects) build correctly.
set(HR_NRF5340_PERIPHERALS_DTSI
  ${ZEPHYR_BASE}/dts/arm/nordic/nrf5340_cpuapp_peripherals.dtsi)

if(EXISTS ${HR_NRF5340_PERIPHERALS_DTSI})
  file(STRINGS ${HR_NRF5340_PERIPHERALS_DTSI} HR_HFCLKAUDIO_NODE_MATCH
    REGEX "^[ \t]*hfclkaudio:[ \t]*hfclkaudio[ \t]*\\{")

  if(HR_HFCLKAUDIO_NODE_MATCH)
    set(app_EXTRA_DTC_OVERLAY_FILE
      ${CMAKE_CURRENT_LIST_DIR}/boards/nrf5340dk_nrf5340_cpuapp-hfclkaudio.overlay
      CACHE INTERNAL ""
    )
  endif()
endif()
