include config.mk

define removeDuplicates 
	$(eval unique :=)
 	$(foreach \
		item,
		$1, \
		$(if $(filter $(item), $(unique)), , $(eval unique += $(item))) \
	)
 	$(unique)
endef 

COMPONENT_CONFIGS := $(foreach \
	component, \
	$(COMPONENTS), \
	$(component)/config.mk \
)
include $(COMPONENT_CONFIGS)

MKDIR := @mkdir -p
RM := @rm -rf
CC := arm-none-eabi-gcc
CXX := arm-none-eabi-g++
OBJCOPY := arm-none-eabi-objcopy
SAM_BA := bossac

c_paths_raw := $(foreach src, $(C_SOURCES), $(dir $(src)))
C_PATHS := $(strip $(call removeDuplicates, $(c_paths_raw)))
C_BUILD_PATHS := $(foreach \
	path, \
	$(C_PATHS), \
	$(join $(BUILD_DIR), $(path))\
)

cpp_paths_raw := $(foreach src, $(CPP_SOURCES), $(dir $(src)))
CPP_PATHS := $(strip $(call removeDuplicates, $(cpp_paths_raw)))
CPP_BUILD_PATHS := $(foreach \
	path, \
	$(CPP_PATHS), \
	$(join $(BUILD_DIR), $(path)) \
)

build_paths_raw := $(CPP_BUILD_PATHS) $(C_BUILD_PATHS)
BUILD_PATHS := $(strip $(call removeDuplicates, $(build_paths_raw)))
src_paths_raw := $(CPP_PATHS) $(C_PATHS)
SRC_PATHS := $(strip $(call removeDuplicates, $(src_paths_raw)))
VPATH = $(SRC_PATHS) #
$(info $(VPATH))

C_BUILD_OBJECTS := $(foreach \
	src, \
	$(C_SOURCES), \
	$(addprefix $(BUILD_DIR), $(patsubst %.c, %.o, $(src))) \
)

CPP_BUILD_OBJECTS := $(foreach \
	src, \
	$(CPP_SOURCES), \
	$(addprefix $(BUILD_DIR), $(patsubst %.cpp, %.o, $(src))) \
)

INCLUDE_PATHS := $(addprefix -I, $(HEADERS))
LINKER_PATHS := $(addprefix -L, $(LIBRARY_PATHS))
COMPILE_SYMBOLS := $(addprefix -D, $(SYMBOLS))
 
BUILD_DEPS := $(patsubst %.o, %.d, $(C_BUILD_OBJECTS))

BUILD_TARGET := $(addprefix $(BUILD_DIR), $(addsuffix .elf, $(TARGET)))
BUILD_TARGET_MAP := $(addprefix $(BUILD_DIR), $(addsuffix .map, $(TARGET)))
BUILD_TARGET_BIN := $(addprefix $(BUILD_DIR), $(addsuffix .bin, $(TARGET)))
BUILD_TARGET_HEX := $(addprefix $(BUILD_DIR), $(addsuffix .hex, $(TARGET)))

MACHINE_FLAGS := \
	-mthumb \
	-mcpu=cortex-m3 

CC_OPTIONS := \
	$(C_OPTIMIZE) \
	$(C_WARNING) \
	$(C_DEBUG) \
	-x c \
	-std=gnu11 \
	-ffunction-sections \
	-ffunction-sections \
	-nostdlib \
	--param max-inline-insns-single=500 \
	-Dprintf=iprintf \

CXX_OPTIONS := \
	$(CPP_OPTIMIZE) \
	$(CPP_WARNING) \
	$(CPP_DEBUG) \
	-x c++ \
	-std=c++11 \
	-ffunction-sections \
	-fdata-sections \
	-nostdlib \
	-fno-threadsafe-statics \
	-fno-rtti \
	-fno-exceptions \

ASM_OPTIONS := \
	$(C_DEBUG) \
	-x assembler-with-cpp \

undef_libs := $(addprefix \
	-u, \
	_sbrk \
	link \
	_close \
	_fstat \
	_isatty \
	_lseek \
	_read \
	_write \
	_exit \
	kill \
)

LINKER_OPTIONS := \
	$(MACHINE_FLAGS) \
	$(LINKER_OPTIMIZE) \
	-Wl,--gc-sections \
	-T$(LINKER_SCRIPT) \
	-Wl,-Map=$(BUILD_TARGET_MAP) \
	-Wl,--cref \
	-Wl,--check-sections \
	-Wl,--gc-sections \
	-Wl,--entry=Reset_Handler \
	-Wl,--unresolved-symbols=report-all \
	-Wl,--warn-common \
	-Wl,--warn-section-align \
	-Wl,--start-group \
	$(OBJECTS) \
	$(ARCHIVES) \
	$(undef_libs) \
	-Wl,--end-group \
	-lm \
	-lgcc

ifdef TOOLCHAIN_PATH
CC_OPTIONS += -nostdinc
CXX_OPTIONS += -nostdinc
LINKER_OPTIONS += \
	-nostartfiles \
	-nodefaultlibs \
	-nostdlib 
cc_inc := \
	arm-none-eabi/include \
	arm-none-eabi/include/machine \
	lib/gcc/arm-none-eabi/6.3.1/include \
	lib/gcc/arm-none-eabi/6.3.1/include-fixed 
cxx_inc := \
	arm-none-eabi/include/c++/6.3.1 \
	arm-none-eabi/include/c++/6.3.1/arm-none-eabi \
	$(cc_inc) 
CC_TOOLCHAIN_INCLUDE_PATHS := $(addprefix -I$(TOOLCHAIN_PATH)/, $(cc_inc))
CXX_TOOLCHAIN_INCLUDE_PATHS := $(addprefix -I$(TOOLCHAIN_PATH)/, $(cxx_inc))
lib_paths := \
	arm-none-eabi/lib \
	lib/gcc/arm-none-eabi/6.3.1
#	arm-none-eabi/lib/thumb 
TOOLCHAIN_LINKER_PATHS := $(addprefix -L$(TOOLCHAIN_PATH)/, $(lib_paths))
endif 

debug_log := $(if $(DEBUG_LOGFILE), $(DEBUG_LOGFILE), /dev/null)
JLINK_SERVER := .jlink.pid
JLINK_SERVER_PID := $(shell pidof JLinkGDBServer)
JLINK_SERVER_CLOSE := $(if $(JLINK_SERVER_PID), kill -9, @echo)
 
define buildObjectFromC
$(1)%.o: %.c
	@echo Building $$@ from $$<
	$(CC) -c \
		$(MACHINE_FLAGS) \
		$(CC_OPTIONS) \
		$(INCLUDE_PATHS) \
		$(CC_TOOLCHAIN_INCLUDE_PATHS) \
		$(COMPILE_SYMBOLS) \
		-o $$@ $$< -MMD
endef

define buildObjectFromCPP
$(1)%.o: %.cpp
	@echo Building $$@ from $$<
	$(CXX) -c \
		$(MACHINE_FLAGS) \
		$(CXX_OPTIONS) \
		$(INCLUDE_PATHS) \
		$(CXX_TOOLCHAIN_INCLUDE_PATHS) \
		$(COMPILE_SYMBOLS) \
		-o $$@ $$< -MMD 
endef

define buildObjectFromASM
$(1)%.o: %.S
	@echo Building $$@ from $$<
	$(CC) -c \
		$(MACHINE_FLAGS) \
		$(INCLUDE_PATHS) \
		$(COMPILE_SYMBOLS) \
		-o $$@ $$< -MMD
endef 

.PHONY: all build_dir flash debug_init jlink_stop jlink

all: build_dir $(BUILD_TARGET) $(BUILD_TARGET_BIN) $(BUILD_TARGET_HEX) debug_init jlink

$(foreach \
	target, \
	$(C_BUILD_PATHS), \
	$(eval $(call buildObjectFromC, $(target))) \
)

$(foreach \
	target, \
	$(CPP_BUILD_PATHS), \
	$(eval $(call buildObjectFromCPP, $(target))) \
)

$(foreach \
	target, \
	$(ASM_BUILF_PATHS), \
	$(eval $(call buildObjectFromASM, $(target))) \
)

-include $(BUILD_DEPS)

$(BUILD_TARGET) : $(C_BUILD_OBJECTS) $(CPP_BUILD_OBJECTS)
	@echo Building $(BUILD_TARGET) ...
	$(CXX) -o $@ $^ \
		$(strip \
			$(LINKER_OPTIONS) \
			$(LINKER_PATHS) \
			$(TOOLCHAIN_LINKER_PATHS) \
		)

$(BUILD_TARGET_BIN) : $(BUILD_TARGET)
	@echo Building $@ from $<
	$(OBJCOPY) -O binary $< $@

$(BUILD_TARGET_HEX) : $(BUILD_TARGET)
	@echo Building $@ from $<
	$(OBJCOPY) -O ihex $< $@


build_dir: $(BUILD_DIR) $(BUILD_PATHS)
$(BUILD_DIR):
	$(MKDIR) $@
$(BUILD_PATHS):
	$(MKDIR) $@

flash:
	@stty -F $(FLASH_PORT) 1200
	$(SAM_BA) --port=ttyACM0 -U false -e -w -v -b $(BUILD_TARGET_BIN) -R
	@stty -F /dev/ttyACM0 115200   

$(DEBUG_CONFIG):
	@echo -e "#Auto generated by Makefile. Do not modify." > $@
	@echo -e "#If you must, modify $(DEBUG_CONFIG) recipe in Makefile" >> $@
	@echo -e "target remote localhost:$(DEBUG_PORT)" >> $@
	@echo -e "monitor device $(DEBUG_DEVICE)" >> $@
	@echo -e "monitor speed $(DEBUG_SPEED)" >> $@
	@echo -e "file $(BUILD_TARGET)" >> $@
	@echo -e "define run" >> $@
	@echo -e "\tload" >> $@
	@echo -e "\tmonitor reset" >> $@
	@echo -e "\tcontinue" >>$@
	@echo -e "define r" >> $@
	@echo -e "\rcontinue" >> $@
	@echo -e "add-auto-load-safe-path" $(PWD)/$@ >> ~/.gdbinit

jlink: $(JLINK_SERVER)
$(JLINK_SERVER):
#	gdb_jlink.sh $(DEBUG_DEVICE) $(DEBUG_SPEEC) $(DEBUG_INTERFACE) $(debug_log) $@
	#Close existing server before starting a new one
#	($(JLINK_SERVER_CLOSE)) #Evaluates to empty string if not running
	$(JLINK_SERVER_CLOSE) $(JLINK_SERVER_PID)
	JLinkGDBServer \
		-device $(DEBUG_DEVICE) \
		-speed $(DEBUG_SPEED) \
		-if $(DEBUG_INTERFACE) \
		> $(debug_log) 2>&1 &
	pidof JLinkGDBServer > $@
	
gdb: $(DEBUG_CONFIG)
	gdb

jlink_stop:
	@kill -9 `cat $(JLINK_SERVER)`; rm $(JLINK_SERVER)

clean:
	$(RM) $(BUILD_DIR)
