C_SOURCES += \
	libs/xbee_ansic_library/src/xbee/xbee_device.c \
	libs/xbee_ansic_library/src/xbee/xbee_socket.c \
	libs/xbee_ansic_library/src/util/swapbytes.c \
	libs/xbee_ansic_library/src/xbee/xbee_atcmd.c \
	libs/paho.mqtt.embedded-c/MQTTPacket/src/MQTTPacket.c \
	libs/paho.mqtt.embedded-c/MQTTPacket/src/MQTTConnectClient.c \
	libs/paho.mqtt.embedded-c/MQTTPacket/src/MQTTSerializePublish.c \
	libs/paho.mqtt.embedded-c/MQTTPacket/src/MQTTDeserializePublish.c \
	libs/paho.mqtt.embedded-c/MQTTPacket/src/MQTTSubscribeClient.c \

CPP_SOURCES += \
	libs/xbee_ansic_library/ports/arduino-due/xbee_platform_arduino_due.cpp \
	libs/xbee_ansic_library/ports/arduino-due/xbee_serial_arduino_due.cpp \
	
HEADERS += \
	libs \
	libs/xbee_ansic_library/include \
	libs/xbee_ansic_library/ports/arduino-due \
	libs/paho.mqtt.embedded-c/MQTTPacket/src \
	libs/static_queue \

SYMBOLS += \
	XBEE_PLATFORM_HEADER="\"platform_config_arduino_due.h\"" \
	XBEE_CELLULAR_ENABLED=1
 
