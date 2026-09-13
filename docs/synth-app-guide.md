# Making a synth repo pipeline-ready

How an openFrameworks video-synth repository becomes compatible with the
synth-build firmware pipeline, so it can ship a downloadable Raspberry Pi OS
image that boots straight into the app.

**Reference implementation:** [PeteHaughie/Spectral-Mesh](https://github.com/PeteHaughie/Spectral-Mesh)
(private). Where this guide says "see SpectralMesh", that repo is the concrete,
tested example of the pattern.

The guide is written for two readers:
- a **human** configuring a synth by hand, and
- an **agent** onboarding a synth into the pipeline (the checklist in §7 is the
  executable spec).

---

## 1. The contract at a glance

The pipeline consumes a single thing from an app repo: a **`firmware-*` GitHub
release** carrying a runtime tarball plus checksums. Everything else the image
needs is derived from it at build time.

Release assets (see SpectralMesh `release-rpi.yml`):

| asset | purpose |
|---|---|
| `SynthName-linux-arm64.tar.gz` | the runtime artifact (see layout below) |
| `SynthName-linux-arm64` | the bare executable (unused by the image, kept for parity) |
| `SHA256SUMS` | sha256 of the tarball + executable — the pipeline verifies against it |

Tarball layout — **runtime files only**, no `.git`, credentials, compilers, or
headers:

```text
SynthName/
  bin/<executable>                 headless GLES/GBM build
  bin/data/shaders/GLES2/          the Pi shaders (GLSL ES 1.00)
  bin/data/shaders/GLSL3/          desktop shaders (GLSL 1.50) — not shipped to the Pi
  bin/data/splash.png              optional boot splash
```

**Hard requirement:** the executable must be a **headless GLES/GBM build** that
renders to `/dev/dri/card0` with no X server. A GLFW/X11 desktop build cannot
run on the headless image.

---

## 2. Build shape

The app must be built as an ARM64/GLES target with a GBM/DRM window. On
openFrameworks this means patching OF 0.12.1's `linuxaarch64` build (which is
otherwise the desktop GLFW/X11 target). SpectralMesh versions these patches in
`.github/patches/of-0.12.1/`:

| patch | effect |
|---|---|
| `0001-ofConstants-rpi-bcm-host-guard.patch` | guard `bcm_host.h` include so `TARGET_RASPBERRY_PI` can be defined on Trixie without firmware headers |
| `0002-linuxaarch64-rpi-gles-config.patch` | `config.linuxaarch64.default.mk`: `TARGET_RASPBERRY_PI` + `TARGET_OPENGLES` + `TARGET_LINUX_ARM`, link GBM/DRM |
| `0003a/b-ofAppEGLWindow-gbm.patch` | a GBM/DRM window in `ofAppEGLWindow` (kmscube pattern): `/dev/dri/card0`, `gbm_surface_create`, EGL, `drmModeSetCrtc`/`PageFlip`, mode selection |
| `0004-ofAppRunner-no-glfw-on-rpi.patch` | exclude the GLFW `ofSetupOpenGL` path when `TARGET_RASPBERRY_PI` is defined |

Applied by **`scripts/rpi/apply-of-patches.sh`** (idempotent, refuses a
non-0.12.1 OF tree) — the same script drives the Docker CI build and a native
Pi build, so both produce identical trees.

`src/main.cpp` needs a `TARGET_OPENGLES` branch:

```cpp
#ifdef TARGET_OPENGLES
    ofGLESWindowSettings settings;
    settings.setSize(720, 480);
    settings.glesVersion = 2;
    settings.windowMode = OF_GAME_MODE;
#else
    // desktop GLFW/GL build (development only)
#endif
```

Docker build deps (SpectralMesh `linux-arm64.Dockerfile`): `libegl-dev
libgles-dev libgbm-dev libdrm-dev libx11-dev libsystemd-dev` + the normal OF
build stack. The artifact stage copies `bin/` (executable + data).

---

## 3. Runtime shape

Three things the app must do at runtime for the image to work:

**Shader routing.** On the Pi the app reads shaders from
`~/.spectral-mesh/shaders/GLES2/` (the home of the service user — in the image
that is `/var/lib/video-synth`); on desktop from `bin/data/shaders/GLSL3/`. The
shader manager resolves `shaders/<name>` to the platform folder and injects the
right `#version` (`#version 100` + `precision highp float;` on the Pi,
`#version 150` on desktop). See SpectralMesh `ShaderManager::loadSource()`.

**Rendering.** The GBM window opens `/dev/dri/card0`, creates a GBM surface +
EGL context, selects a 720×480p mode on the connector, and page-flips. The
service user needs `video`, `render`, and `audio` groups (the firmware adds
these). The app takes DRM master — one instance at a time.

**Watchdog contract.** The image runs the app as a systemd unit of
`Type=notify` with `WatchdogSec=10`. The app must use `sd_notify`:
- send `READY=1` once startup (GL/window init) completes, and
- send `WATCHDOG=1` every frame (in the render loop).

See SpectralMesh `src/SystemdWatchdog.cpp` (wraps `sd_notify`, no-op outside
Linux). Without the pings, systemd kills the app after 10s; without `READY=1`,
the unit times out. This is what makes the synth crash-resilient.

The app stays dual-target: the same source builds for macOS/desktop (GLSL3)
and the Pi (GLES2).

---

## 4. The firmware manifest

Once the app publishes a compatible release, a per-synth manifest in
`firmware/images/` describes it (copy `spectral-mesh.yml`). Every field:

| field | purpose |
|---|---|
| `name` | image/app name (e.g. `spectral-mesh`) |
| `firmware_version` | this image's version; also the GitHub release tag (`firmware-<version>`) |
| `architecture` | `arm64` |
| `base.pi_gen_ref` / `base.image_name` | pi-gen branch (`arm64`) and image base name |
| `application.name` / `.repo` / `.release` / `.artifact` | where the app artifact lives |
| `application.command` | canonical exec path, `/opt/video-synth/app/<name>` |
| `application.runtime_packages` | Debian packages the binary needs (read its `DT_NEEDED`) |
| `network.wifi_ssid` / `.wifi_psk` / `.wifi_country` | WiFi to auto-join at first boot (for SSH debug) |
| `boot.splash` / `.autostart` / `.console` | kiosk vs development toggles |

**Reading `DT_NEEDED`:** `objdump -p <binary>` (or `readelf -d`) lists the
dynamic libraries; map each soname to its Debian package via
`packages.debian.org` (e.g. `libEGL.so.1` → `libegl1`, `libGLESv2.so.2` →
`libgles2`, `libgbm.so.1` → `libgbm1`, `libdrm.so.2` → `libdrm2`,
`libsystemd.so.0` → `libsystemd0`, `libasound.so.2` → `libasound2t64`, …).
The pipeline installs these into the image at build time.

**Dev-loop toggles:**
- `boot.autostart: false` → boot to a console, app started manually with
  `sudo systemctl start video-synth` (no display lockout while debugging).
- `boot.console: true` → `console=tty1` stays, so boot text + app output are
  visible; keep plymouth happy (it needs the fbcon).
- `boot.splash: false` → no boot splash (boot text visible).
- `network.wifi_*` → the image auto-joins your test WiFi so SSH works.

---

## 5. What the firmware handles for you

The app repo does **not** need to do any of these — the pipeline bakes them in:

- Canonical install of the executable to `/opt/video-synth/app/<name>`.
- Seeding the `GLES2` shaders into the service user's home
  (`/var/lib/video-synth/.spectral-mesh/shaders/GLES2/`), and trimming `GLSL3`
  + the rest of `bin/data` from the image.
- Creating the `video-synth` service user with `video`, `render`, `audio`
  groups.
- The `Type=notify` + `WatchdogSec` systemd unit, auto-restart, and
  `multi-user.target` autostart.
- The plymouth boot splash (from `bin/data/splash.png`), WiFi
  auto-connection, cloud-init removal, and the `pi` login user with SSH.

So a synth repo stays thin: build the headless artifact, publish the release,
and the pipeline turns it into a bootable image.

---

## 6. Adapting a new synth — checklist

For a human or an agent, in order:

1. **Confirm headless GLES/GBM** — the app is, or can be made, a headless
   GLES/GBM build that renders to `/dev/dri/card0` with no X.
2. **Adopt the OF patches** — copy SpectralMesh's `.github/patches/of-0.12.1/`
   + `scripts/rpi/apply-of-patches.sh`; add the GLES/GBM/systemd Docker deps.
3. **Add the GLES branch** to `main.cpp` (`ofGLESWindowSettings`,
   `glesVersion=2`, 720×480p).
4. **Split the shaders** into `bin/data/shaders/GLES2/` and `GLSL3/`; route via
   the shader manager; use `~/.spectral-mesh/shaders/GLES2/` on the Pi.
5. **Add the sd_notify watchdog** (`READY=1` after startup, `WATCHDOG=1` per
   frame).
6. **Add `bin/data/splash.png`** if you want a boot splash.
7. **Wire CI** — build the arm64 artifact in Docker, publish a `firmware-*`
   release with the tarball + `SHA256SUMS` (reference SpectralMesh
   `release-rpi.yml`).
8. **Write the manifest** in synth-build (copy `spectral-mesh.yml`): set
   `application.*`, derive `runtime_packages` from `DT_NEEDED`, set `network`
   + `boot` to your test network.
9. **Dev loop** — build with `boot.autostart: true` + `boot.console: true`
   (+ `boot.splash: false` to see boot text), burn, SSH in, debug. When solid,
   set `boot.splash: true` (+ `boot.autostart: true`) for production.
10. **Iterate** — each app change: tag a new `firmware-*` release, bump
    `application.release` here (or pass `APP_RELEASE`), build.

---

## 7. The iteration loop

App code change → tag a `firmware-*` release on the app repo → update
`application.release` in the manifest (or `APP_RELEASE`) → run
`build-image.sh` (or dispatch the workflow) → a new image is published as
`firmware-<version>` with the versioned `.img.xz`, SHA-256 sidecar, SBOM, and
manifest.

The handshake is deliberately **not** automatic: the manifest pins the app
release so a new app release never silently changes what images get built.
