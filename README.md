ZoiteChat Debian packaging (starter kit)

This folder was generated from Debian's HexChat packaging and then renamed/adjusted for ZoiteChat.

What it gives you:
- debian/ directory with package names, install paths, AppStream ID, and VCS fields updated to ZoiteChat
- Debian-specific patches/default-server overrides removed to reduce patch-fail risk on the fork
- Python plugin package moved to Suggests (since it's known to be unstable in Flatpak builds)

Build on Fedora (recommended via container)
------------------------------------------
1) From your ZoiteChat repo root (the one that contains meson.build), copy this debian/ directory into it.

2) Build inside a Debian container using Podman:

    podman run --rm -it \
      -v "$PWD":/src -w /src \
      debian:testing bash

3) In the container:

    apt-get update
    apt-get install -y --no-install-recommends \
      build-essential devscripts equivs ca-certificates

    # Install build-deps from debian/control
    mk-build-deps -i -t "apt-get -y --no-install-recommends" debian/control

    # Build (binary packages)
    dpkg-buildpackage -us -uc -b

Your .deb files will appear in the parent directory of the repo on the host.

Notes
-----
- If ZoiteChat still uses HexChat’s AppStream IDs or installs plugin metainfo,
  tweak debian/*.install accordingly.
- If you want the Python plugin excluded entirely, remove the zoitechat-python3
  package stanza from debian/control and delete debian/zoitechat-python3.* files.
