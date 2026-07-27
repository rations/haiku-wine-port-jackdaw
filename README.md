# haiku-wine-port

Haiku patches for **Wine 11.8**, packaged as a HaikuPorts port. They exist so that
[vstbridge-haiku](https://github.com/rations/vstbridge-haiku) can run **Windows VST2 and VST3
plug-ins** inside [jackDAW-haiku](https://github.com/rations/jackDAW-haiku) — the stock
HaikuPorts Wine package is missing the Haiku fixes the bridge depends on.

**This repository is the source of truth for these patches.** They were offered upstream and
declined (HaikuPorts does not accept AI-assisted contributions), so they are maintained here
instead. Nothing else is authoritative — not the build machine, not any working tree. See
[Where things live](#where-things-live).

The repo carries no Wine sources, only the recipe and the patchsets (~450 KB). `haikuporter`
downloads the upstream tarball (checksum pinned in the recipe) and applies the patchset to it.

## Layout

```
app-emulation/wine/            drop-in replacement for the haikuports port directory
  wine-11.8.recipe             REVISION=7
  patches/
    wine-11.8.patchset         the 10 patches below (this is the deliverable)
    wine-11.8-home.patchset    optional, not applied by the recipe (see Optional patches)
  additional-files/
    wine.rdef.in               Deskbar icon/version resource template
docs/
  OSMESA-GL-PLAN.md            design + test plan for the software-GL work (patch 9)
  probes/                      standalone programs establishing that plan's facts
attic/                         kept for reference, not built
  winehaiku.drv-11.8-abandoned.patch
  wine-7.1.recipe
  wine-7.1.patchset
```

## The patchset

Ten patches. **1–5 are upstream HaikuPorts', unchanged**, with original authorship preserved in
each `From:` line. **6–10 are local.**

| # | Patch | Author | Why |
|---|---|---|---|
| 1 | Add Wayland input header | X512 | upstream |
| 2 | 11.8 patches | X512 | upstream — the bulk of the Haiku port |
| 3 | Define `get_current_thread_data` | Peppersawce | upstream |
| 4 | Disable Bluetooth on Beta 5 | Peppersawce | upstream |
| 5 | Opensound support | Ken Mays | upstream — audio |
| 6 | Run static initializers in Winelib modules on Haiku | local | Winelib modules never ran their C++ static constructors. vstbridge's Wine host is a C++ Winelib module, so without this it starts with uninitialised globals. **This is the essential one.** |
| 7 | Do not block in the SIGSEGV handler on Haiku | local | The port's handler ended in a blocking `fgetc(stdin)` so a developer could inspect the fault. Anything without a usable stdin — a DAW, any GUI app, anything from the Deskbar — hung there instead of letting Wine's handler recover. |
| 8 | loader: find the loader path via `find_path()` on Haiku | local | Haiku has no `/proc`, so `get_self_exe()` returned NULL and the loader resolved `argv[0]` relative to the current directory — a bare `wine` from `PATH` failed with `could not load ntdll.so`. Fixed with `find_path(B_FIND_PATH_IMAGE_PATH)`. |
| 9 | winewayland: add OSMesa software OpenGL backend for Haiku | local | Gives Wine a software GL backend (OSMesa/llvmpipe) so wined3d can create a device and D3D-drawn plug-in editors can render — **without it those editors do not display at all**. Haiku's Mesa has no DRI swrast and no Wayland EGL platform, so `winewayland.drv`'s existing EGL path can never initialize there. Confirmed rendering; see the gate table below for what is and is not verified. |
| 10 | dxgi: wait for a refresh interval in `IDXGIOutput::WaitForVBlank()` | local | `WaitForVBlank()` was a stub printing a FIXME and returning `E_NOTIMPL` immediately. JUCE 8's Direct2D renderer drives all repainting from a dedicated thread that calls it in a loop, so with no wait that thread spun — measured at 62,024 calls in ~25 s, saturating a core — and the editor rendered one frame then never responded to input again. Now sleeps to the next refresh-interval boundary. Not Haiku-specific: the same gap is why these plug-ins are reported as needing DXVK on Linux, DXVK shipping its own DXGI with a real vblank wait. |
| 11 | ntdll: debug — name the faulting caller by scanning the stack | local | **Development instrumentation, not a fix.** Prints module-relative return addresses found above SP on SIGSEGV, so a call through a null pointer names its caller. Kept last so it can be dropped without disturbing anything before it. |

Patch 8 removes the need for the `export WINELOADER=/boot/system/bin/wine` workaround earlier
Haiku Wine builds required in `~/config/settings/profile`: winegcc's generated `.exe` wrapper
scripts fall back to invoking a bare `wine`, which now works from any directory.

### Dropping the debug patch

Patch 11 writes to stdout on every fault. To build without it, delete the last patch from
`patches/wine-11.8.patchset` (everything from its `From <sha>` line to end of file) and bump
`REVISION`. Nothing else references it.

### Status of the OSMesa backend (patch 9)

**Confirmed working for the case it was written for.** Patch 9 is what makes Direct3D-drawn
plug-in editors render at all: without it the Nembrini Audio VST3 editors did not display,
with it they do. Rendering has held since REVISION 6; since REVISION 8, which adds patch 10,
those editors are also interactive.

Against the five gates in [docs/OSMESA-GL-PLAN.md](docs/OSMESA-GL-PLAN.md) §5:

| Gate | Status |
|---|---|
| 1 — standalone `osprobe` shows GL 4.5 / llvmpipe | **passing** |
| 2 — Wine GL smoke test (`wglCreateContext`, `glGetString`) | never run as a discrete test; entailed by 3–5 passing |
| 3 — wined3d creates a device against llvmpipe | **passing** — the old `wined3d_caps_gl_ctx_create Failed to find a suitable pixel format` is gone |
| 4 — rendered frame reaches the floating Wayland window | **passing** — editors display and are usable; no formal colour/orientation check was performed |
| 5 — NA VST3 in jackDAW: D3D GUI renders **and is interactive** | **passing, 2026-07-27** (REVISION 8) |
| 5b — repeated open/close of an editor leaves `jackd` and jackDAW running | **passing, 2026-07-27** — the forced-restart symptom is gone |

Gate 2 is the only one never exercised on its own, so a regression in the raw WGL path would
first surface as a failure in one of the gates below it rather than as a clear GL error.

Patch 11, the debug stack scanner, dates from debugging this WGL path and is not part of the
fix. The verified groundwork, and why the alternatives (DXVK/Vulkan WSI, porting Mesa's
Wayland-EGL platform) are dead ends on Haiku, is all in that document.

Patch 9 is gated at build time on `HAVE_OSMESA`, which `configure` derives from `GL/osmesa.h`
plus `libOSMesa`. **If those are missing the port still builds — silently, without software
GL**, falling back to the EGL path that cannot work on Haiku. The recipe now declares
`devel:libOSMesa` and `lib:libOSMesa` so this cannot happen by accident; it did not before
REVISION 7.

### Optional patches

`patches/wine-11.8-home.patchset` (Peppersawce, "Move wine dir to `/config/settings`") is
**not** listed in the recipe's `PATCHES` and is not applied. It is upstream's and marked buggy;
kept only so it is not lost.

## Building

On the target Haiku machine, with a [haikuports](https://github.com/haikuports/haikuports)
checkout and `haikuporter` configured:

```sh
# 1. Overlay this port onto your haikuports checkout
cp -a app-emulation/wine/. ~/haikuports/app-emulation/wine/

# 2. Build (roughly an hour; builds both i386 and x86_64)
cd ~/haikuports
haikuporter -y -j8 wine-11.8
```

The package lands in `~/haikuports/packages/wine-11.8-8-x86_64.hpkg`. When rebuilding over a
previous attempt, `haikuporter -y -c wine-11.8` first to clean the work directory — otherwise
the old, already-patched source tree is reused.

Build dependencies are declared in the recipe (`BUILD_REQUIRES`/`BUILD_PREREQUIRES`) and include
llvm21 + clang + lld for the PE cross-build; `haikuporter` will tell you what is missing.

### Iterating without an hour-long rebuild

A clean rebuild is ~55 minutes, which is not a per-edit cycle. Two things make a fast loop
possible:

- `haikuporter` applies `.patchset` files **with git**, so `work-11.8/sources/wine-wine-11.8`
  is a git repo whose `HEAD` is the pristine tarball plus these patches. Editing it and
  rebuilding one target inside the chroot (`haikuporter -E wine-11.8`, then
  `make -j8 dlls/dxgi/dxgi.dll`) takes minutes, and `git diff` there is exactly your change.
- Nothing needs to be installed to test it. Wine sets its internal `build_dir` when its
  `ntdll.so` sits under `<tree>/dlls/ntdll`, and then omits the installed `/boot/system/lib/wine`
  from the DLL search path entirely — so the build tree wins outright:

  ```sh
  export WINELOADER=<work tree>/loader/wine
  ```

  (`WINEDLLPATH` does *not* work for this: its entries are appended *after* the installed
  directory, so an installed builtin always wins.)

> **The catch: `haikuporter -c` deletes `work-11.8/sources/` without warning.** An edit that
> exists only there is one command from being lost — that has happened twice here. Replay every
> such edit into the patchset (`git diff` in the work tree, apply it to the tree the patchset is
> generated from) *before* any clean rebuild. Every source change ultimately belongs in the
> patchset — see below.

### Installing

```sh
pkgman install ./wine-11.8-8-x86_64.hpkg
```

- **x86_64**, and **Haiku hrev59867 or newer** (`requires: haiku>=r1~beta6_hrev59867-1`).
  `haikuporter` bakes the build machine's revision in as a floor, so an older nightly refuses to
  install it. Newer is fine — this is a userland package, not a kernel add-on.
- `lib:libfreetype`, `lib:libOSMesa`, `lib:libusb` and `lib:libvulkan`, which `pkgman` pulls
  from HaikuPorts automatically. If the target machine is offline, bring those along too.

## Changing a patch

Do not edit the patchset by hand. Rebuild it from a git tree, which is how the current one was
produced:

```sh
# One-time: reconstruct the working tree from this repo
tar xf wine-11.8.tar.gz && cd wine-wine-11.8
git init -q && git add -A && git commit -qm "wine-11.8 pristine"
git am /path/to/haiku-wine-port/app-emulation/wine/patches/wine-11.8.patchset

# Then, for every change: edit, commit, regenerate, commit the patchset here
git format-patch --stdout <pristine-sha>..HEAD \
  > /path/to/haiku-wine-port/app-emulation/wine/patches/wine-11.8.patchset
```

Each commit is one patch, in order. Bump `REVISION` in the recipe whenever the patchset or
recipe changes — never reuse a published revision.

To verify a regenerated patchset reproduces the tree it came from:

```sh
git worktree add --detach /tmp/verify <pristine-sha>
cd /tmp/verify && git am /path/to/patches/wine-11.8.patchset
git rev-parse HEAD^{tree}   # must equal the dev tree's HEAD^{tree}
```

## Where things live

| | Role |
|---|---|
| **this repo** | **Authoritative.** The recipe and patchsets, in git. If it is not committed here, it is not kept. |
| `third_party/wine-osmesa-dev` | Scratch working tree for editing source and regenerating the patchset (the `git am` tree above). Regenerable; safe to delete and recreate. Tag `pre-consolidate-backup-20260726` marks the pre-cleanup history. |
| haikulaptop `~/haikuports/app-emulation/wine/` | Build machine only. Copy the port in, build, copy the `.hpkg` out. Has no git history of its own — anything edited only there is one `haikuporter -c` from gone. |

## Graphics driver

The port builds `--with-wayland --without-x`. Which driver a prefix uses is set once, per
prefix, in `HKCU\Software\Wine\Drivers\Graphics`, and the right choice depends on what you are
doing:

| Setting | Result |
|---|---|
| `null` | **Use this for plug-ins.** vstbridge captures the plug-in's window and repaints it inside jackDAW's FX window, so the plug-in's own on-screen window is an unwanted duplicate. The `null` driver declines to create a driver window surface, which sends win32u down its offscreen path — the window still draws and still receives `WM_PAINT`, but never reaches a display. |
| `winewayland.drv` (default) | A floating window for any Windows program. For a bridged plug-in you get that floating window *in addition to* the copy in the FX window. Keep this if the prefix also runs ordinary Windows applications, since `null` leaves those with no window at all. |

```sh
wine reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f
```

vstbridge logs a warning naming the current driver whenever it is not `null`.

Note that patch 9's software GL lives in `winewayland.drv`, so it is only in play when that
driver is selected — the `null` driver has no GL backend at all.

## Revision history

| REVISION | Notes |
|---|---|
| 8 | Adds patch 10 (`dxgi: WaitForVBlank`). Built on the laptop 2026-07-27 (hrev59899, `EXIT=0`, `HAVE_OSMESA 1` confirmed in the work tree's `config.h`). **Observed working:** Direct3D-drawn plug-in editors — Nembrini Audio VST3s under vstbridge in jackDAW — became interactive; before this they rendered one frame and ignored all mouse and keyboard input, including clicks delivered straight to the floating Wayland window. Also folds in REVISION 7's OSMesa dependency declarations, which were never built on their own. |
| 7 | Declares `devel:libOSMesa` / `lib:libOSMesa`. Same sources as 6. **Never built** — superseded by 8 before a package was produced. Verification checklist retained because it has not been performed: after install, verify in order: (a) `/boot/system/bin/wine --version` prints `wine-11.8`; (b) `objdump -T /boot/system/bin/wine \| grep find_path` shows the import — **REV6 shipped without patch 08 compiled in**, so a bare `wine` from PATH failed with `could not load ntdll.so` and needed a `WINELOADER`+`WINEDLLPATH` workaround in `~/config/settings/profile`; (c) a bare `wine --version` works. Only then drop the profile workaround. (`find_path(B_FIND_PATH_IMAGE_PATH)` is verified working on hrev59899, so patch 08 is the fix once actually built in.) |
| 6 | Built on the laptop 2026-07-24; the last package before 8. Its sources are what patches 1–9 and 11 now reproduce — patch 10 is new in REVISION 8. The OSMesa pixel-format fix and the stack scanner had been hand-edited into the build work tree and existed in no patchset until this repo captured them. |
| 3–5 | Superseded. |

## Attic

- `winehaiku.drv-11.8-abandoned.patch` — an in-tree Haiku graphics driver tried during
  development and **abandoned**; it did not work. Deliberately not part of the port. The
  arrangement that does work is *Graphics driver* above.
- `wine-7.1.recipe`, `wine-7.1.patchset` — X512's original Wine 7.1 Haiku port (13 patches),
  the ancestor of this work. Reference only.

## Licensing and credit

Wine is **LGPL v2.1**; these patches are derived work under the same licence. Patches 1–5 are
upstream HaikuPorts' work (including X512's Haiku port) with authorship preserved in each
`From:` line. The recipe is derived from the haikuports `app-emulation/wine` recipe.
