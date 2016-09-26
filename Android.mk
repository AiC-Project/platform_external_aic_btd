LOCAL_PATH := $(call my-dir)

include $(call all-subdir-makefiles)

#############################################
include $(CLEAR_VARS)

LOCAL_CPP_EXTENSION := .cc

LOCAL_SRC_FILES := btd.cc bt.proto

LOCAL_PROTOC_FLAGS := --cpp_out=.

LOCAL_C_INCLUDES	:= bionic \
 			   external/stlport/stlport \
 			   external/aic/libaicd \
 			   device/aicVM/common/libcppsensors_packet \
 			   external/protobuf/src \
   			   external/protobuf/src/google/protobuf \
   			   external/protobuf/src/google/protobuf/stubs \
   			   external/protobuf/src/google/protobuf/io

IGNORED_WARNINGS := -Wno-sign-compare -Wno-unused-parameter -Wno-sign-promo -Wno-error=return-type
LOCAL_CFLAGS := -O2 -fpermissive -Wmissing-field-initializers -DGOOGLE_PROTOBUF_NO_RTTI -DHAS_NO_BDROID_BUILDCFG -ljvm

LOCAL_MODULE := btd
LOCAL_SHARED_LIBRARIES += liblog libcutils libstlport \
 				libandroid_runtime

LOCAL_STATIC_LIBRARIES += libprotobuf-cpp-2.3.0-lite libprotobuf-cpp-2.3.0-full
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

