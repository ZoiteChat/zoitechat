# Troubleshooting

## Flatpak

If `flatpak run net.zoite.Zoitechat` only prints `Gtk-WARNING **: cannot open display`,
collect extra diagnostics with:

```bash
flatpak run --devel --command=sh net.zoite.Zoitechat
```

Then inside that shell:

```bash
echo "DISPLAY=$DISPLAY WAYLAND_DISPLAY=$WAYLAND_DISPLAY XDG_SESSION_TYPE=$XDG_SESSION_TYPE"
xdpyinfo >/tmp/xdpyinfo.log 2>&1 || true
env G_MESSAGES_DEBUG=all zoitechat 2>&1 | tee /tmp/zoitechat-debug.log
```

To inspect sandbox permissions from the host:

```bash
flatpak info --show-permissions net.zoite.Zoitechat
flatpak override --user --show net.zoite.Zoitechat
```

If needed, try running with direct access to your active display stack:

```bash
# X11 sessions
flatpak override --user --socket=x11 net.zoite.Zoitechat

# Wayland sessions
flatpak override --user --socket=wayland net.zoite.Zoitechat
```

You can reset overrides after testing:

```bash
flatpak override --user --reset net.zoite.Zoitechat
```

## Theme palette migration and legacy keys

New builds store theme colors as semantic token keys in `colors.conf` (for example `theme.mode.light.token.mirc_0`).
When loading old configs that only contain legacy keys (`color_*` and `dark_color_*`), ZoiteChat migrates them automatically if `theme.palette.semantic_migrated` is not present.

By default saves write both semantic token keys and legacy keys for compatibility. To phase out legacy writes, set:

```bash
export ZOITECHAT_THEME_WRITE_LEGACY_KEYS=0
```

With that setting, saves write semantic token keys only while legacy-key loading remains available for older config files that have not yet been marked as migrated.
