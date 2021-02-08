
TARGET = test

INCLUDES = ..

I_FLAGS := $(addprefix -I, $(INCLUDES))

HEADERS = $(wildcard $(INCLUDES)/*.h)

FLAGS = \
	-Wall \
	-Wextra \
	-Werror \
	-std=c++14 \
	-g

.PHONY: all run
all: $(TARGET)

$(TARGET): test.cpp $(HEADERS)
	 g++ -o $@ $(I_FLAGS) $(FLAGS) $<

run: $(TARGET)
	valgrind --leak-check=full --track-origins=yes $<
