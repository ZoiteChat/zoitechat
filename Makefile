PREFIX ?= /usr/local
BUILD_DIR ?= build
MESON ?= meson
NINJA ?= ninja
MESON_SETUP_ARGS ?=
MESON_COMPILE_ARGS ?=
MESON_INSTALL_ARGS ?=

.PHONY: all configure build reconfigure install clean distclean

all: build

configure:
	$(MESON) setup $(BUILD_DIR) --prefix=$(PREFIX) $(MESON_SETUP_ARGS)

build: configure
	$(MESON) compile -C $(BUILD_DIR) $(MESON_COMPILE_ARGS)

reconfigure:
	$(MESON) setup --reconfigure $(BUILD_DIR) --prefix=$(PREFIX) $(MESON_SETUP_ARGS)

install: build
	$(MESON) install -C $(BUILD_DIR) $(MESON_INSTALL_ARGS)

clean:
	$(MESON) compile -C $(BUILD_DIR) --clean

distclean:
	rm -rf $(BUILD_DIR)