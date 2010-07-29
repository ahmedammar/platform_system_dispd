LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=                \
                  dispd.c \
                  disp.c \
                  dispmgr.c \
                  switch.c \
                  uevent.c \
                  cmd_dispatch.c

LOCAL_MODULE:= dispd

LOCAL_C_INCLUDES := $(KERNEL_HEADERS)

LOCAL_CFLAGS := 

ifeq ($(TARGET_BOARD_PLATFORM),imx51_BBG)
LOCAL_CFLAGS += -DMX51_BBG_DISPD
endif

LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_EXECUTABLE)
