# =============================================================================
# Lumiscripta — Makefile
# =============================================================================

CXX      ?= g++
CC       ?= gcc
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2
CCFLAGS  := -O2 -Wall

INCDIR   := include
SRCDIR   := src
BUILDDIR := build
TARGET   := lumiscripta

# -----------------------------------------------------------------------------
# Include paths
# -----------------------------------------------------------------------------
INCFLAGS := -I$(INCDIR) \
            -I$(INCDIR)/lumiscripta \
            -Ithird_party \
            -Ithird_party/imgui \
            -Ithird_party/md4c \
            -Ithird_party/md4c/src \
            -Ithird_party/imgui_md

# -----------------------------------------------------------------------------
# Source files
# -----------------------------------------------------------------------------
SRCS := $(wildcard $(SRCDIR)/*.cpp)

# ImGui sources
IMGUI_SRCS := third_party/imgui/imgui.cpp \
              third_party/imgui/imgui_draw.cpp \
              third_party/imgui/imgui_tables.cpp \
              third_party/imgui/imgui_widgets.cpp \
              third_party/imgui/imgui_demo.cpp \
              third_party/imgui/backends/imgui_impl_glfw.cpp \
              third_party/imgui/backends/imgui_impl_opengl3.cpp \
              third_party/imgui/misc/cpp/imgui_stdlib.cpp

# md4c (C source)
MD4C_SRCS := third_party/md4c/src/md4c.c

# imgui_md
IMGUI_MD_SRCS := third_party/imgui_md/imgui_md.cpp

# All objects
OBJS := $(patsubst $(SRCDIR)/%.cpp,$(BUILDDIR)/src/%.o,$(SRCS)) \
        $(patsubst third_party/imgui/%.cpp,$(BUILDDIR)/imgui/%.o,$(IMGUI_SRCS)) \
        $(patsubst third_party/md4c/src/%.c,$(BUILDDIR)/md4c/%.o,$(MD4C_SRCS)) \
        $(patsubst third_party/imgui_md/%.cpp,$(BUILDDIR)/imgui_md/%.o,$(IMGUI_MD_SRCS))

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)

# -----------------------------------------------------------------------------
# Platform-specific linker flags
# -----------------------------------------------------------------------------
ifeq ($(UNAME_S),Linux)
    LDFLAGS += -lglfw -lGLEW -lGL -ldl -lpthread
endif

ifeq ($(UNAME_S),Darwin)
    # macOS with MacPorts: /opt/local/lib
    # macOS with Homebrew: /opt/homebrew/lib (Apple Silicon) or /usr/local/lib (Intel)
    LDFLAGS += -L/opt/local/lib -L/opt/homebrew/lib -L/usr/local/lib
    LDFLAGS += -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreFoundation
    INCFLAGS += -I/opt/local/include -I/opt/homebrew/include -I/usr/local/include
endif

ifneq (,$(findstring MINGW,$(UNAME_S)))
    LDFLAGS += -lglfw3 -lglew32 -lopengl32 -lgdi32 -limm32
endif

# -----------------------------------------------------------------------------
# Rules
# -----------------------------------------------------------------------------
.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) $^ $(LDFLAGS) -o $@

# Project sources
$(BUILDDIR)/src/%.o: $(SRCDIR)/%.cpp | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

# ImGui sources
$(BUILDDIR)/imgui/%.o: third_party/imgui/%.cpp | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

# md4c (C)
$(BUILDDIR)/md4c/%.o: third_party/md4c/src/%.c | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CC) $(CCFLAGS) $(INCFLAGS) -c $< -o $@

# imgui_md
$(BUILDDIR)/imgui_md/%.o: third_party/imgui_md/%.cpp | $(BUILDDIR)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCFLAGS) -c $< -o $@

$(BUILDDIR):
	mkdir -p $(BUILDDIR)/src $(BUILDDIR)/imgui/backends $(BUILDDIR)/imgui/misc/cpp $(BUILDDIR)/md4c $(BUILDDIR)/imgui_md

clean:
	$(RM) -r $(BUILDDIR) $(TARGET)

run: all
	./$(TARGET)