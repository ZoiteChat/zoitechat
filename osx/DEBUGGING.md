# Debugging ZoiteChat on macOS (Xcode + CLI)

If the unsigned `.app` launches but does nothing (or immediately exits), use the steps below to get actionable logs and a debugger session.

## 1) Build with debug symbols

ZoiteChat uses Meson. Build a debug configuration first so LLDB can show useful backtraces.

```bash
meson setup build-macos-debug --buildtype=debug
meson compile -C build-macos-debug
```

This project default is `debugoptimized`, but a full `debug` build is better while diagnosing crashes/startup issues.

## 2) Bundle the app and verify deployment target

Use the existing bundle script:

```bash
cd osx
./makebundle.sh
```

The generated `Info.plist` should keep:

- `LSMinimumSystemVersion = 11.0`

That is the project’s explicit minimum runtime target for macOS 11+.

## 3) Sanity-check the Mach-O binary in the bundle

Confirm architecture(s), deployment target, and linked libraries:

```bash
file ZoiteChat.app/Contents/MacOS/ZoiteChat-bin
otool -l ZoiteChat.app/Contents/MacOS/ZoiteChat-bin | rg -n "LC_BUILD_VERSION|minos|sdk"
otool -L ZoiteChat.app/Contents/MacOS/ZoiteChat-bin
```

For widest compatibility, build universal (`arm64` + `x86_64`) or build separately per arch and test each on matching hosts.

## 4) Ad-hoc sign for local debugging

Unsigned GUI binaries can fail in unhelpful ways because of hardened runtime/quarantine/Gatekeeper interactions. For local debugging, ad-hoc sign the app:

```bash
xattr -dr com.apple.quarantine ZoiteChat.app
codesign --force --deep --sign - ZoiteChat.app
codesign --verify --deep --strict --verbose=2 ZoiteChat.app
```

## 5) Launch from Terminal first (before Xcode)

Direct launch exposes stdout/stderr and validates the launcher environment:

```bash
./ZoiteChat.app/Contents/MacOS/ZoiteChat
```

The launcher script sets GTK/Pango/GDK environment variables. If direct launch fails, capture that output first.

## 6) Debug with LLDB (reliable baseline)

Debug the real executable inside the bundle:

```bash
lldb -- ZoiteChat.app/Contents/MacOS/ZoiteChat-bin
(lldb) run
(lldb) bt
```

If it exits instantly, set breakpoints on startup entry points (for example `main`) and re-run.

## 7) Debug with Xcode (if you prefer GUI)

Xcode works best when opening the executable directly instead of importing build scripts.

1. **File → Open…** and select `ZoiteChat.app/Contents/MacOS/ZoiteChat-bin`.
2. In **Product → Scheme → Edit Scheme…**
   - Executable: `ZoiteChat-bin`
   - Working directory: repo root (or `osx/`)
   - Add environment variables equivalent to the launcher if needed.
3. Run under debugger.

If Xcode “runs” but no UI appears, compare its environment to `osx/launcher.sh` and copy missing GTK-related variables into the scheme.

## 8) Capture macOS crash diagnostics

Even if no dialog appears, macOS usually logs termination reasons:

```bash
log stream --style compact --predicate 'process == "ZoiteChat-bin"'
```

Also check:

- `~/Library/Logs/DiagnosticReports/`

## 9) Frequent root causes for “silent” startup failures

- Missing GTK runtime libraries in app bundle (`otool -L` shows unresolved paths).
- Incorrect `@rpath`/install names after bundling.
- Running under Rosetta mismatch (x86_64 binary with arm64-only deps or vice versa).
- Quarantine/signature issues on unsigned artifacts.
- Missing `GDK_PIXBUF_MODULE_FILE`, `GTK_IM_MODULE_FILE`, or schema paths.


### "Bad CPU type in executable"

This means the bundled `ZoiteChat-bin` architecture does not match the Mac you are running on.

- Intel Mac requires `x86_64`
- Apple Silicon requires `arm64` (or Rosetta + `x86_64`)

Check the binary quickly:

```bash
file ZoiteChat.app/Contents/MacOS/ZoiteChat-bin
```

Build for Intel explicitly when needed:

```bash
export CFLAGS="-arch x86_64"
export LDFLAGS="-arch x86_64"
meson setup build-macos-intel --buildtype=debug
meson compile -C build-macos-intel
```

If your dependency stack supports it, build universal (`arm64` + `x86_64`) and verify with `lipo -info`.

Example with this repo's scripts:

```bash
PREFIX="$(brew --prefix)"

CFLAGS="-arch arm64" LDFLAGS="-arch arm64" meson setup build-macos-arm64 --prefix="$PREFIX"
CFLAGS="-arch x86_64" LDFLAGS="-arch x86_64" meson setup build-macos-x86_64 --prefix="$PREFIX"

CFLAGS="-arch arm64" LDFLAGS="-arch arm64" meson compile -C build-macos-arm64
CFLAGS="-arch x86_64" LDFLAGS="-arch x86_64" meson compile -C build-macos-x86_64

sudo meson install -C build-macos-arm64

cd osx
UNIVERSAL=1 \
UNIVERSAL_BINARIES="../build-macos-arm64/src/fe-gtk/zoitechat ../build-macos-x86_64/src/fe-gtk/zoitechat" \
./makebundle.sh
```

## 10) Recommended compatibility settings for macOS 11+

- Keep `LSMinimumSystemVersion` at `11.0`.
- Build on the oldest macOS SDK/toolchain that still supports your dependencies, or explicitly set:

```bash
export MACOSX_DEPLOYMENT_TARGET=11.0
```

- Verify with `otool -l` that `minos` is actually `11.0` in the final executable.

---

If you want, we can add an Xcode scheme file to this repo that mirrors `osx/launcher.sh` so “Run” in Xcode behaves exactly like launching the app bundle from Finder/Terminal.
