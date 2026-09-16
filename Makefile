# Prince of Persia, native Haiku port. Build on Haiku (x86_gcc2 hybrid) with:
#   setarch x86 make
# On a 64-bit Haiku a plain `make` works as well.
CC ?= gcc
CXX ?= g++
BUILD ?= build
APP = PrinceOfPersia

CPPFLAGS += -Isrc/haiku -D_GNU_SOURCE=1
CFLAGS ?= -O2 -std=gnu99 -Wall -Wno-unused-variable -Wno-unused-function -Wno-unused-but-set-variable -Wno-misleading-indentation
CXXFLAGS ?= -O2 -std=c++14 -Wall -Wextra -Wno-unused-parameter -Wno-multichar
LIBS = -lbe -lmedia -ltranslation -lroot -lm

ENGINE_SRC = $(wildcard src/engine/*.c)
HAIKU_SRC = $(wildcard src/haiku/*.cpp)
ENGINE_OBJ = $(patsubst src/engine/%.c,$(BUILD)/engine_%.o,$(ENGINE_SRC))
HAIKU_OBJ = $(patsubst src/haiku/%.cpp,$(BUILD)/haiku_%.o,$(HAIKU_SRC))
HEADERS = $(wildcard src/engine/*.h) $(wildcard src/haiku/SDL2/*.h)

.PHONY: all clean install uninstall
all: $(BUILD)/$(APP)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/engine_%.o: src/engine/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/haiku_%.o: src/haiku/%.cpp $(HEADERS) | $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

$(BUILD)/$(APP): $(ENGINE_OBJ) $(HAIKU_OBJ) $(BUILD)/$(APP).rsrc
	$(CXX) $(ENGINE_OBJ) $(HAIKU_OBJ) -o $@ $(LIBS)
	xres -o $@ $(BUILD)/$(APP).rsrc
	resattr -o $@ $(BUILD)/$(APP).rsrc
	mimeset -f $@

$(BUILD)/$(APP).rsrc: resources/$(APP).rdef | $(BUILD)
	rc -o $@ $<

resources/$(APP).rdef: tools/make_icon.py
	python3 tools/make_icon.py

install: $(BUILD)/$(APP)
	./install.sh

uninstall:
	./install.sh --uninstall

clean:
	rm -f $(ENGINE_OBJ) $(HAIKU_OBJ) $(BUILD)/$(APP) $(BUILD)/$(APP).rsrc
