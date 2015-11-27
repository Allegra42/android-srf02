
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := sensors.omap4
LOCAL_MODULE_FILENAME := libsensors_omap4

LOCAL_MODULE_PATH := $(TARGET_OUT_SHARED_LIBRARIES)/hw
LOCAL_CFLAGS := -DLOG_TAG=\"Sensors\"
#LOCAL_CFLAGS += ggdb
#LOCAL_STRIP_MODULE = false
#LOCAL_CFLAGS += -DINVENSENSE_COMPASS_CAL #needed??
#LOCAL_C_INCLUDES += hardware/invensense/65xx/libsensors_iio

LOCAL_SRC_FILES :=  \
	SensorBase.cpp\
	InputEventReader.cpp\
	sensors.cpp \
	proximity_sensor.cpp
	
	
LOCAL_SHARED_LIBRARIES := liblog

LOCAL_MODULE_TAGS := eng #for test with eng?

include $(BUILD_SHARED_LIBRARY)

