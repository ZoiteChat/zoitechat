# Native macOS build and distribution

This builds GTK3 with the native Quartz backend, macOS menus/Dock integration,
UserNotifications, translations, bundled emoji, TLS, image loaders and supported
native plugins. It creates a self-contained `ZoiteChat.app`; end users do not
need Homebrew, XQuartz or Python. No system-wide ZoiteChat installation occurs.

The source changes are conditional on Darwin. Windows and Linux retain their
existing notification backends and feature defaults. This adds a separate
macOS workflow; it does not replace existing workflows.

## 1. Install build prerequisites

Use a Mac with a currently supported macOS release and native Homebrew. On Apple
Silicon, use ARM Homebrew (`/opt/homebrew`), not Intel Homebrew under Rosetta.
Intel builds use Intel Homebrew (`/usr/local`). Install Xcode command-line tools
and Homebrew from https://brew.sh if absent. Full current Xcode is recommended
for distribution and needed to meet the current App Store submission SDK rules.

```sh
xcode-select --install
# Run once if the command-line tools are not installed; let installation finish.

cd /path/to/zoitechat
brew bundle --file packaging/macos/Brewfile
python3 packaging/macos/build.py
open dist/ZoiteChat.app
```

The build compiles the app, runs its Meson tests, stages an installation within
`build-macos/macos-stage`, bundles and rewrites every non-system Mach-O dependency,
creates the icon and plist, audits the result, and signs it ad hoc for local use.
It refuses to overwrite an existing output app. For another build, choose a new
output path, or explicitly delete your old generated app before rebuilding:

```sh
python3 packaging/macos/build.py --output dist/ZoiteChat-test2.app --build-number 2
```

The same build directory can be reconfigured. Use a different build directory
for another architecture or the experimental store variant. Do not change
architectures within one Meson build directory.

Homebrew packages change over time; this is a repeatable procedure, not a
bit-for-bit reproducible dependency lock. The bundle records dependency versions
in `Contents/Resources/homebrew-dependencies.json`. Retain that record and the
release's source commit. For a reproducible release, archive the exact dependency
sources and build them in a controlled prefix, or maintain pinned Homebrew formulae.

## 2. Verify portability and behavior

```sh
python3 -m unittest discover -s packaging/macos/tests -v
python3 packaging/macos/bundle.py audit dist/ZoiteChat.app
python3 packaging/macos/smoke.py dist/ZoiteChat.app
```

The smoke check copies the app to a path containing spaces, verifies its signature,
runs `--version`, and keeps an isolated GUI process alive for ten seconds without
connecting to IRC. It tests startup, not interaction or long-running stability.

The audit verifies the entire dependency closure, including GTK image/input
modules, GIO TLS, Enchant providers, OpenSSL providers and ZoiteChat plugins. It
fails on missing libraries, external symlinks/search paths, architecture mismatches
or unresolved load commands. The minimum macOS version in the plist is derived
from **all** bundled binaries, including Homebrew libraries. Lowering that plist
value does not make newer dependency bottles run on an older Mac.

Before releasing, test on a second Mac without Homebrew installed:

- Launch from Finder and from a relocated/read-only location; verify Dock and
  menu bar, preferences persistence, dark/light themes and normal shutdown.
- Connect with verified TLS, exercise SASL authentication and reconnect.
- Send Unicode/emoji, render SVG/PNG/JPEG icons/images, and test HTTPS image previews.
- Verify clipboard shortcuts, input methods, notifications/permission denial,
  detach/reattach tabs, focus changes, unread highlights and notification clicks.
- Send/receive/resume DCC, write downloads and logs, import/export themes/settings.
- Test FiSH, Lua and DCC checksum plugins in the direct build.
- Exercise accessibility with VoiceOver and keyboard-only navigation.

Direct builds include Lua, FiSH and checksum, and retain the plugin loader.
Python/Perl are explicitly not bundled: reliable redistribution requires packaging
and auditing their interpreter runtimes, standard libraries and extensions.
Linux `/proc`/PCI sysinfo is not built on macOS. These are macOS build choices;
other platforms' plugin defaults are unchanged. macOS uses the existing password
storage fallback; this patch does not add a Keychain backend. Audit credentials
storage before adopting a stronger macOS security claim.

Desktop URL registration (`irc://` in Finder/browser) is not advertised by the
plist; command-line IRC URLs continue to work. A native open-URL event handler
and single-instance forwarding need separate integration before registering those
schemes. AppIndicator and Linux D-Bus scripting/single-instance support are not
used on macOS.

## 3. Sign and notarize for direct downloads

Local ad-hoc builds are for testing; they are not notarized releases. For public
release, enroll in Apple's Developer Program, install your **Developer ID
Application** certificate with its private key, then sign using that identity.
Use your actual certificate identity in place of the example:

```sh
python3 packaging/macos/bundle.py sign dist/ZoiteChat.app \
  --identity 'Developer ID Application: YOUR NAME (TEAMID)' \
  --entitlements packaging/macos/direct.entitlements

# Store credentials once in your login Keychain; enter your own values interactively.
xcrun notarytool store-credentials ZoiteChat-notary

ditto -c -k --keepParent dist/ZoiteChat.app dist/ZoiteChat-notarization.zip
xcrun notarytool submit dist/ZoiteChat-notarization.zip \
  --keychain-profile ZoiteChat-notary --wait

# Only after notarization has succeeded:
xcrun stapler staple dist/ZoiteChat.app
xcrun stapler validate dist/ZoiteChat.app
spctl --assess --type execute --verbose=2 dist/ZoiteChat.app

ditto -c -k --keepParent dist/ZoiteChat.app dist/ZoiteChat-macos.zip
```

Signing runs inside-out, verifies the final bundle and enables hardened runtime.
It does not use `codesign --deep` to sign. `--deep` is used only for verification.
The direct entitlement allows users' independently signed native plugins; it
relaxes library validation, not the entire hardened runtime. No JIT or executable
memory entitlements are added. Do not change bundle contents after signing; signing
again is required after any code/resource edit. Stapling is Apple's supported
post-signing operation.

Publish exact corresponding source for ZoiteChat and satisfy the applicable
licenses of each bundled dependency. Notices and dependency metadata are included,
but those alone do not satisfy every source-distribution obligation. Review the
notarization log if rejected; the script does not silently bypass Gatekeeper.

### Automate signing and notarization with GitHub Actions

The workflow now has a separate signed/notarized release job for both
architectures. It uses Developer ID distribution for direct downloads, not Mac
App Store signing. You can manage it from Windows/Linux; GitHub's macOS runners
perform all signing, notarization and stapling.

Prepare these repository Actions secrets under **Settings → Secrets and
variables → Actions**, or store them as environment secrets in **macos-release**.
The certificate setting names are unchanged from the original instructions.
The three `APPLE_API_*` settings are new notarization credentials:

| Name | Type | Value |
| --- | --- | --- |
| `MACOS_CERTIFICATE_P12_BASE64` | Secret | Base64-encoded Developer ID Application `.p12`, including its private key |
| `MACOS_CERTIFICATE_PASSWORD` | Secret | Password used to export that `.p12` |
| `MACOS_SIGNING_IDENTITY` | Variable, or Secret | Exact `Developer ID Application: YOUR NAME (TEAMID)` identity; the matching certificate's SHA-1 fingerprint is also accepted |
| `APPLE_API_KEY_P8` | Secret | Entire plain-text App Store Connect **Team API key** `.p8` file, including PEM header/footer and newlines |
| `APPLE_API_KEY_ID` | Secret | That API key's Key ID |
| `APPLE_API_ISSUER_ID` | Secret | The Team API key's Issuer ID UUID; this is **not** your Apple Team ID |

Repository variables are under the **Variables** tab. If the identity is entered
as both a variable and a secret, the variable takes precedence. No separate
keychain password is needed; the job generates a temporary random password.
The `.p8` is the notarization API key; it does not replace the signing `.p12`.
No Apple ID password or app-specific password is used by this configuration.

To obtain the API key, open **App Store Connect → Users and Access → Integrations
→ App Store Connect API → Team Keys → Generate API Key**. Choose **Developer**
access for notarization. If API access is not enabled, the Account Holder must
request it first. Download the `.p8` and record the Key ID and Issuer ID. Use a
**Team** key with this workflow: individual API keys use different authentication
and are not supported by this issuer-based configuration. Apple allows the
private key download only once, so retain your copy securely.

For the certificate, export the Developer ID Application identity (certificate
plus private key) from Keychain Access as a password-protected `.p12`.
If you generated the CSR with OpenSSL on Linux/WSL, you can also create it there.
Assuming Apple's downloaded DER certificate has been saved as
`developerID_application.cer`, and you retained `zoitechat-signing.key`:

```sh
openssl x509 -inform DER -in developerID_application.cer \
  -out zoitechat-signing.pem
openssl pkcs12 -export -inkey zoitechat-signing.key \
  -in zoitechat-signing.pem -out ZoiteChat-signing.p12

# Enter the existing private-key password, then choose the .p12 export password.
# Copy the resulting Base64 text into MACOS_CERTIFICATE_P12_BASE64:
openssl base64 -A -in ZoiteChat-signing.p12 -out ZoiteChat-signing.p12.base64

# Read the certificate's commonName to obtain MACOS_SIGNING_IDENTITY:
openssl x509 -in zoitechat-signing.pem -noout -subject -nameopt multiline
```

Upload only the public CSR to Apple. Put the `.p12` Base64, export password and
API credentials in GitHub settings, never in the repository or build artifacts.

Once these patches are committed to `master` and the settings are ready:

1. Open **Actions → macOS build → Run workflow**.
2. Select **master** and enable **Sign with Developer ID and notarize using
   release credentials**.
3. Run the workflow. It creates separate Apple Silicon and Intel release jobs.
4. After success, download `ZoiteChat-macos-arm64-signed-notarized` or
   `ZoiteChat-macos-x86_64-signed-notarized`. The ZIP inside contains the signed,
   stapled app. Actions artifacts wrap it in their own download archive.

A push of a `v*` tag also triggers the signed release jobs automatically, so wait
until credentials are configured before pushing a release tag. Normal branch
pushes, pull requests and manual runs with the checkbox off continue to build
ad-hoc test artifacts without any signing secrets. Manual credentialed releases
are restricted to `master`; pull requests cannot invoke the release job.
Missing/malformed release settings fail before the Homebrew/build steps rather
than producing an unsigned artifact marked as a release.

The release jobs use the GitHub environment **macos-release**. Repository secrets
work with it; environment secrets can override them. Configure environment
branch/tag restrictions or required reviewers if that fits your repository's
release process. This workflow does not automatically create a GitHub Release,
publish to zoitechat.org, or upload to App Store Connect; it produces downloadable
signed/notarized artifacts for you to publish.

The job imports the `.p12` into a temporary keychain, installs Apple's public
Developer ID intermediate certificates, verifies a valid matching identity,
and signs inside-out with hardened runtime and the direct plugin entitlement.
It uses the existing bundle audit and then submits a ZIP with `notarytool`.
After Apple reports **Accepted**, it staples the ticket to the `.app`, verifies
stapling/signatures and Gatekeeper acceptance, and creates a **new** final ZIP.
Private credential files and the temporary keychain are cleaned up on normal
success/failure paths; hosted runner VMs are also discarded after the job.

The default Apple wait limit is **60 minutes** within a **180-minute** release
job. A rejected, failed or still-pending submission produces no final release
ZIP. The `ZoiteChat-macos-<arch>-notarization-log` artifact retains the submission
ID, status and Apple's available diagnostic log. First submissions can take
longer; check the saved ID with `notarytool info` and your API credentials. An
accepted submission still requires stapling and repackaging before publication;
rerunning this workflow starts a new build and submission. To increase the wait
limit, update `--notary-timeout` and allow adequate job timeout for build plus wait.

Both architecture builds receive the same bundle build number for one workflow
run. The API credentials are supplied only to the config-check/release steps,
not to Homebrew, compiler tests, ordinary builds or pull-request jobs.

## 4. Mac App Store: experimental groundwork, not submission-ready

A working direct-download app is not sufficient for App Store submission. This
repository inherits GPL-2.0-or-later code from XChat/HexChat and other contributors.
App Store terms and GPL obligations require a licensing review; do not assume
that signing, a custom EULA, or relicensing only your new changes resolves rights
to inherited code. This patch changes no licenses or copyright permissions.

An isolated sandbox-test build is provided:

```sh
python3 packaging/macos/build.py --store-experimental \
  --build-dir build-macos-store --output dist/ZoiteChat-store-test.app \
  --build-number 1

# Local sandbox experimentation (not an uploadable distribution signature):
python3 packaging/macos/bundle.py sign dist/ZoiteChat-store-test.app \
  --identity - --entitlements packaging/macos/store-experimental.entitlements
open dist/ZoiteChat-store-test.app
```

The separate store variant excludes loadable plugins and scripting runtimes.
The template grants sandboxed client/server networking, user-selected files and
Downloads. The direct version retains its plugin functionality. This is an
explicit experimental variant, not a change to Windows/Linux or direct builds.

Before a store package can be regarded as a release candidate:

1. Resolve the inherited GPL/dependency-license distribution question and review
   Apple's current SDK, content/chat moderation, privacy and accessibility rules.
2. Register `org.Zoitechat` (or change the plist identifier consistently if already
   owned by another developer), create App Store Connect metadata, and choose a
   unique incrementing build number. Use a stable release, not a `-dev` snapshot.
3. Test sandboxed settings/logs, notifications, SASL/TLS, HTTPS and DCC on a real
   Mac. GTK dialogs are not assumed to grant Powerbox access. Add native
   `NSOpenPanel`/`NSSavePanel` integration and security-scoped bookmarks for
   persistent access to user-selected files. The entitlement template alone does
   not solve persistent file access. `/EXEC`, custom external commands, user
   plugin loading and arbitrary external paths cannot be promised under sandbox.
4. Add required privacy manifest declarations based on an actual API audit of the
   app and dependencies; do not submit an invented empty compliance declaration.
5. Obtain matching App Store distribution application/installer certificates and
   a matching provisioning profile. Generate team-specific signing entitlements
   from the profile with Xcode; do not use Developer ID or the generic local-test
   template as a store distribution identity/profile.
6. Embed the matching profile as `Contents/embedded.provisionprofile` **before**
   final signing. Sign embedded libraries with the distribution identity, then
   the app with its profile-authorized entitlements (use `bundle.py sign` with
   that identity and generated entitlement file). Package with Apple's
   `productbuild`, validate and upload with Transporter/App Store Connect, then
   test the App Store processed build via TestFlight before submission.

Example **after those prerequisites are satisfied**:

```sh
# Use the actual prepared profile and team-specific entitlements:
cp /path/to/ZoiteChat.provisionprofile \
  dist/ZoiteChat-store-test.app/Contents/embedded.provisionprofile
python3 packaging/macos/bundle.py sign dist/ZoiteChat-store-test.app \
  --identity 'YOUR APP STORE APPLICATION CERTIFICATE IDENTITY' \
  --entitlements /path/to/ZoiteChat-distribution.entitlements
productbuild --component dist/ZoiteChat-store-test.app /Applications \
  --sign 'YOUR APP STORE INSTALLER CERTIFICATE IDENTITY' dist/ZoiteChat-store.pkg
```

No App Store approval, sandbox compatibility or legal compatibility is asserted.
For an initial macOS release, the signed/notarized direct build is the practical
path while those issues are resolved.

## 5. CI and architecture support

`.github/workflows/macos-build.yml` adds independent Apple Silicon and Intel jobs.
Ordinary jobs install prerequisites, run tests, create/audit/ad-hoc-sign the
bundle, check relocated startup and upload test ZIPs. They need no Apple secrets.
The separate release jobs add Developer ID signing, Apple notarization, stapling
and final verification. Only their successful output is named signed/notarized.

Homebrew may build unsupported Intel bottles from source; that can be slower.
The workflow uses native runners and does not claim a universal application.
Do not merge only the main executable with `lipo`: a universal release must merge
and validate every bundled dylib, module and plugin plus matching resources.
Signing must happen after that merge. Publish separate architecture downloads
until a full universal dependency pipeline is available.

## References

- GTK on macOS: https://www.gtk.org/docs/installations/macos
- GTK macOS backend: https://docs.gtk.org/gtk3/macos.html
- Apple distribution signing: https://developer.apple.com/documentation/xcode/creating-distribution-signed-code-for-the-mac
- Apple packaging: https://developer.apple.com/documentation/xcode/packaging-mac-software-for-distribution
- GitHub certificate import: https://docs.github.com/en/actions/how-tos/deploy/deploy-to-third-party-platforms/sign-xcode-applications
- Apple notarization authentication: https://developer.apple.com/documentation/technotes/tn3147-migrating-to-the-latest-notarization-tool
- App Store Connect API keys: https://developer.apple.com/help/app-store-connect/get-started/app-store-connect-api/
- Apple review rules (especially 1.2 and 2.4.5): https://developer.apple.com/app-store/review/guidelines/
- Apple sandbox file access: https://developer.apple.com/documentation/security/accessing-files-from-the-macos-app-sandbox
- FSF discussion of GPL and App Store terms (historical, not a current legal determination): https://www.fsf.org/blogs/licensing/more-about-the-app-store-gpl-enforcement
