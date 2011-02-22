LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                \
                  dispd.c \
                  disp.c \
                  dispmgr.c \
                  switch.c \
		  dvi_detection.c \
		  hdmi_detection.c \
                  uevent.c \
                  cmd_dispatch.c

LOCAL_MODULE:= dispd

LOCAL_C_INCLUDES := $(KERNEL_HEADERS)

LOCAL_CFLAGS := 

ifeq ($(BOARD_SOC_TYPE),IMX51)
ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),BBG)
LOCAL_CFLAGS += -DMX51_BBG_DISPD
endif
endif

ifeq ($(BOARD_SOC_TYPE),IMX53)
ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),SMD)
LOCAL_CFLAGS += -DMX53_SMD_DISPD
endif
endif

LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_MODULE_TAGS := eng
include $(BUILD_EXECUTABLE)
