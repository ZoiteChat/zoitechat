PREFIX ?= /usr/local
BUILD_DIR ?= build
MESON ?= meson
NINJA ?= ninja
C_STD ?= c17
MESON_SETUP_ARGS ?=
MESON_SETUP_ARGS += -Dc_std=$(C_STD)
MESON_COMPILE_ARGS ?=
MESON_INSTALL_ARGS ?=

CONFIG_STAMP := $(BUILD_DIR)/build.ninja

.PHONY: all configure build reconfigure install uninstall clean distclean

all: build

# Only run initial meson setup if we don't have a configured build dir yet.
$(CONFIG_STAMP):
	@mkdir -p $(BUILD_DIR)
	@env NINJA=$(NINJA) $(MESON) setup $(BUILD_DIR) --prefix=$(PREFIX) $(MESON_SETUP_ARGS)

configure: $(CONFIG_STAMP)

build: configure
	@env NINJA=$(NINJA) $(MESON) compile -C $(BUILD_DIR) $(MESON_COMPILE_ARGS)

reconfigure:
	@mkdir -p $(BUILD_DIR)
	@env NINJA=$(NINJA) $(MESON) setup --reconfigure $(BUILD_DIR) --prefix=$(PREFIX) $(MESON_SETUP_ARGS)

install: build
	@env NINJA=$(NINJA) $(MESON) install -C $(BUILD_DIR) $(MESON_INSTALL_ARGS)

uninstall: configure
	@$(NINJA) -C $(BUILD_DIR) uninstall

clean:
	@if [ -f "$(CONFIG_STAMP)" ]; then \
		env NINJA=$(NINJA) $(MESON) compile -C $(BUILD_DIR) --clean; \
	else \
		echo "Nothing to clean (no $(CONFIG_STAMP))."; \
	fi

distclean:
	rm -rf $(BUILD_DIR)
