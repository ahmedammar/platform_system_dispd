LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                \
                  dispd.c \
                  disp.c \
                  dispmgr.c \
                  switch.c \
		  hdmi_detection.c \
                  uevent.c \
                  cmd_dispatch.c

LOCAL_MODULE:= dispd

LOCAL_C_INCLUDES := $(KERNEL_HEADERS)

LOCAL_CFLAGS := 

ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),imx51_bbg)
LOCAL_CFLAGS += -DMX51_BBG_DISPD
endif

ifeq ($(TARGET_BOOTLOADER_BOARD_NAME),imx53_smd)
LOCAL_CFLAGS += -DMX53_SMD_DISPD
endif

LOCAL_SHARED_LIBRARIES := libcutils
LOCAL_MODULE_TAGS := eng
include $(BUILD_EXECUTABLE)
