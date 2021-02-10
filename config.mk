TARGET := hello-xbee

CPP_SOURCES := \
	./gb4xbee.cpp \
	./xbee_notify.cpp \
	./gb4mqtt.cpp \
	./main.cpp \

HEADERS := ./

LIBRARY_PATHS := 

LINKER_SCRIPT := arduino/variants/arduino_due_x/linker_scripts/gcc/flash.ld
#TOOLCHAIN_PATH := $(HOME)/.local/share/atmel-arm-toolchain/gnu

CPP_OPTIMIZE := -Os
C_OPTIMIZE := -Os
LINKER_OPTIMIZE := -Os
CPP_WARNINGS := -Wall -Wextra
C_WARNINGS := -Wall -Wextra
CPP_DEBUG := -g
C_DEBUG := -g

BUILD_DIR := build/

COMPONENTS := \
	arduino \
	libs \

date_command := \"$(shell \
	python -c \
	"from datetime import datetime; \
	print(datetime.utcnow().strftime('%Y-%m-%d,%H:%M'))" \
)\"
machine_command := \"$(shell \
	echo $(USER)@$(HOSTNAME) \
)\"
SYMBOLS := \
	_GNU_SOURCE \
	BUILD_DATE="$(date_command)" \
	BUILD_MACHINE="$(machine_command)" \
	SERIAL_DEBUG \


FLASH_PORT := /dev/ttyACM0
FLASH_BASE_ADDRESS := 0x80000

DEBUG_CONFIG := .gdbinit
DEBUG_FLASH_CMD := .flash.jlink
DEBUG_PORT := 2331
DEBUG_DEVICE := ATSAM3X8E
DEBUG_SPEED := auto
DEBUG_INTERFACE := SWD
DEBUG_LOGFILE := .jlink.log
