# =============================================================================
# Lumiscripta — Makefile
# =============================================================================

CXX      ?= g++
CC       ?= gcc

# Dear ImGui font backends.
# - IMGUI_ENABLE_FREETYPE replaces the built-in stb_truetype rasterizer with
#   FreeType (see third_party/imgui/misc/freetype/). It is what allows colour
#   glyphs ('COLR' v0 emoji) to be rasterized as RGBA and packed in the atlas.
# - IMGUI_USE_WCHAR32 makes ImWchar a 32-bit type so codepoints above the BMP
#   (emoji live at U+1F000+) can be represented at all. Without it emoji are
#   simply impossible.
# These defines must be identical for every translation unit that includes
# imgui.h (imgui.cpp, imgui_draw.cpp and our own sources), otherwise the ImWchar
# and ImFontLayout differ between TUs (ODR violation).
CXXFLAGS := -std=c++17 -Wall -Wextra -pedantic -O2 \
            -DIMGUI_ENABLE_FREETYPE -DIMGUI_USE_WCHAR32
CCFLAGS  := -O2 -Wall

# FreeType discovery. pkg-config covers Linux and Homebrew/Intel-mac; the
# fallback covers MacPorts and Homebrew-on-Apple-Silicon where pkg-config may
# not be installed or may not be on the path.
FREETYPE_CFLAGS := $(shell pkg-config --cflags freetype2 2>/dev/null)
FREETYPE_LIBS   := $(shell pkg-config --libs freetype2 2>/dev/null)
ifeq ($(strip $(FREETYPE_LIBS)),)
    FREETYPE_CFLAGS := -I/opt/local/include/freetype2 -I/opt/homebrew/include/freetype2 -I/usr/local/include/freetype2
    FREETYPE_LIBS   := -L/opt/local/lib -L/opt/homebrew/lib -L/usr/local/lib -lfreetype
endif

INCDIR   := include
SRCDIR   := src
BUILDDIR := build
TARGET   := lumiscripta

# Where `make install` puts things. The binary resolves its assets as
# $(PREFIX)/share/lumiscripta/assets (see assetDirs() in include/lumiscripta/utils.h).
PREFIX   ?= /usr/local

# -----------------------------------------------------------------------------
# Include paths
# -----------------------------------------------------------------------------
INCFLAGS := -I$(INCDIR) \
            -I$(INCDIR)/lumiscripta \
            -Ithird_party \
            -Ithird_party/imgui \
            -Ithird_party/md4c \
            -Ithird_party/md4c/src \
            -Ithird_party/imgui_md \
            $(FREETYPE_CFLAGS)

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
              third_party/imgui/misc/cpp/imgui_stdlib.cpp \
              third_party/imgui/misc/freetype/imgui_freetype.cpp

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
    LDFLAGS += -lglfw -lGLEW -lGL -ldl -lpthread $(FREETYPE_LIBS)
endif

ifeq ($(UNAME_S),Darwin)
    # macOS with MacPorts: /opt/local/lib
    # macOS with Homebrew: /opt/homebrew/lib (Apple Silicon) or /usr/local/lib (Intel)
    LDFLAGS += -L/opt/local/lib -L/opt/homebrew/lib -L/usr/local/lib
    LDFLAGS += -lglfw -framework OpenGL -framework Cocoa -framework IOKit -framework CoreFoundation
    LDFLAGS += $(FREETYPE_LIBS)
    INCFLAGS += -I/opt/local/include -I/opt/homebrew/include -I/usr/local/include
endif

ifneq (,$(findstring MINGW,$(UNAME_S)))
    LDFLAGS += -lglfw3 -lglew32 -lopengl32 -lgdi32 -limm32 $(FREETYPE_LIBS)
endif

# -----------------------------------------------------------------------------
# Rules
# -----------------------------------------------------------------------------
.PHONY: all clean run install

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

# Install the binary and its assets side by side, so the app works no matter
# which directory it is launched from:
#   $(PREFIX)/bin/lumiscripta
#   $(PREFIX)/share/lumiscripta/assets/
install: all
	install -d $(DESTDIR)$(PREFIX)/bin $(DESTDIR)$(PREFIX)/share/lumiscripta
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)
	cp -R assets $(DESTDIR)$(PREFIX)/share/lumiscripta/
	# Desktop integration (icons for taskbars/docks where the window icon is
	# not taken from the executable): the .desktop entry names the icon
	# 'lumiscripta', which is installed into the hicolor theme.
	install -d $(DESTDIR)$(PREFIX)/share/applications
	install -m 644 assets/lumiscripta.desktop $(DESTDIR)$(PREFIX)/share/applications/lumiscripta.desktop
	install -d $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps
	install -m 644 assets/branding/logo-light.png $(DESTDIR)$(PREFIX)/share/icons/hicolor/256x256/apps/lumiscripta.png

# macOS: regular windows cannot have runtime icons, so the taskbar/dock icon
# can only come from an app bundle. This packages Lumiscripta.app with the
# logo converted to .icns (unverified locally — needs macOS + sips/iconutil).
macos-bundle: all
	@rm -rf Lumiscripta.app
	@mkdir -p Lumiscripta.app/Contents/MacOS Lumiscripta.app/Contents/Resources
	@cp $(TARGET) Lumiscripta.app/Contents/MacOS/
	@cp -R assets Lumiscripta.app/Contents/Resources/
	@printf '<?xml version="1.0" encoding="UTF-8"?>\n<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">\n<plist version="1.0">\n<dict>\n\t<key>CFBundleName</key>\n\t<string>Lumiscripta</string>\n\t<key>CFBundleExecutable</key>\n\t<string>lumiscripta</string>\n\t<key>CFBundleIdentifier</key>\n\t<string>com.lausdeo.lumiscripta</string>\n\t<key>CFBundleIconFile</key>\n\t<string>lumiscripta</string>\n\t<key>LSMinimumSystemVersion</key>\n\t<string>10.13</string>\n</dict>\n</plist>\n' > Lumiscripta.app/Contents/Info.plist
	@mkdir -p /tmp/lumiscripta-icon.iconset
	@for px in 16 32 128 256 512; do sips -z $$px $$px assets/branding/logo-light.png --out /tmp/lumiscripta-icon.iconset/icon_$${px}x$${px}.png >/dev/null; done
	@iconutil -c icns /tmp/lumiscripta-icon.iconset -o Lumiscripta.app/Contents/Resources/lumiscripta.icns
	@rm -rf /tmp/lumiscripta-icon.iconset
	@echo 'Lumiscripta.app built (macOS only; needs sips+iconutil).'