#
# Main Makefile. This is basically the same as a component makefile.
#
# This Makefile should, at the very least, just include $(SDK_PATH)/make/component_common.mk. By default, 
# this will take the sources in the src/ directory, compile them and link them into 
# lib(subdirectory_name).a in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

COMPONENT_SRCDIRS += \
	ble_hid \
	hal \
	helper \
	function_tasks

COMPONENT_ADD_INCLUDEDIRS += \
	ble_hid \
	hal \
	helper \
	function_tasks
CFLAGS += -D LOG_LOCAL_LEVEL=ESP_LOG_DEBUG

# include $(IDF_PATH)/make/component_common.mk
