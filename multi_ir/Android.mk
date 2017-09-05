LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:= multi_ir.c multi_ir.h
LOCAL_MODULE := multi_ir
LOCAL_MODULE_TAGS := optional
LOCAL_FORCE_STATIC_EXECUTABLE := true
LOCAL_STATIC_LIBRARIES := libcutils libc liblog
include $(BUILD_EXECUTABLE)

