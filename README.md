# haiku-wine-port

A patched build of Wine for Haiku, packaged as a HaikuPorts port. It exists so that
[vstbridge-haiku](https://github.com/rations/vstbridge-haiku) can run **Windows VST2 and VST3
plug-ins** inside [jackDAW-haiku](https://github.com/rations/jackDAW-haiku) — the stock
HaikuPorts Wine package is missing three Haiku fixes the bridge depends on.

This repository is **only the port** — the recipe and the patchset, about 100 KB. It carries no
Wine sources: `haikuporter` downloads the upstream tarball (checksum pinned in the recipe) and
applies the patchset to it.

## What is different from upstream HaikuPorts

The port is `app-emulation/wine` from [haikuports](https://github.com/haikuports/haikuports)
with three patches appended to `wine-11.8.patchset` and the revision bumped to 3. The first five
patches in the patchset are upstream's, unchanged.

| Patch | Why it is needed |
|---|---|
| Run static initializers in Winelib modules on Haiku | Winelib modules never ran their C++ static constructors. vstbridge's Wine host is a C++ Winelib module, so without this it starts with uninitialised globals. **This is the essential one.** |
| Do not block in the SIGSEGV handler on Haiku | A blocking operation inside the SIGSEGV handler could deadlock the faulting thread instead of letting Wine's handler recover. |
| loader: find the loader path via `find_path()` on Haiku | Haiku has no `/proc`, so `get_self_exe()` returned NULL and the loader fell back to resolving `argv[0]`. A bare `wine` from `PATH` was then resolved relative to the current directory and failed with `could not load ntdll.so`. Fixed with `find_path(B_FIND_PATH_IMAGE_PATH)`. |

The last one removes the need for the `export WINELOADER=/boot/system/bin/wine` workaround that
earlier Haiku Wine builds required in `~/config/settings/profile`: winegcc's generated `.exe`
wrapper scripts fall back to invoking a bare `wine`, which now works from any directory.

An in-tree `winehaiku.drv` graphics driver was tried during development and **abandoned** — it
did not work, and it is deliberately not part of this port. See *Graphics driver* below for the
arrangement that does work.

## Installing the prebuilt package

A built `wine-11.8-3-x86_64.hpkg` is attached to this repository's release. Install it with:

```sh
pkgman install ./wine-11.8-3-x86_64.hpkg
```

Requirements, read off the built package:

- **x86_64**, and **Haiku hrev59867 or newer** (`requires: haiku>=r1~beta6_hrev59867-1`).
  `haikuporter` bakes the build machine's revision in as a floor, so an older nightly will
  refuse to install it. Newer is fine — this is a userland package, not a kernel add-on.
- `lib:libfreetype`, `lib:libusb` and `lib:libvulkan`, which `pkgman` pulls from HaikuPorts
  automatically. If the target machine is offline, bring those packages along too.

## Building it yourself

Needed for a different Haiku revision, or if you would rather not trust a binary. On the target
machine, with a [haikuports](https://github.com/haikuports/haikuports) checkout and
`haikuporter` configured:

```sh
# 1. Overlay this port onto your haikuports checkout
cp -a app-emulation/wine/. ~/haikuports/app-emulation/wine/

# 2. Build it (expect roughly an hour; it builds both i386 and x86_64)
cd ~/haikuports
haikuporter -y -j8 wine-11.8
```

The package lands in `~/haikuports/packages/wine-11.8-3-x86_64.hpkg`. If you are rebuilding
over a previous attempt, `haikuporter -y -c wine-11.8` first to clean the work directory —
otherwise the old, already-patched source tree is reused.

Build dependencies are declared in the recipe (`BUILD_REQUIRES`/`BUILD_PREREQUIRES`) and include
llvm21 + clang + lld for the PE cross-build; `haikuporter` will tell you what is missing.

## Graphics driver

The port builds `--with-wayland --without-x`. Which driver a prefix uses is set once, per prefix,
in `HKCU\Software\Wine\Drivers\Graphics`, and the right choice depends on what you are doing:

| Setting | Result |
|---|---|
| `null` | **Use this for plug-ins.** vstbridge captures the plug-in's window and repaints it inside jackDAW's FX window, so the plug-in's own on-screen window is an unwanted duplicate. The `null` driver declines to create a driver window surface, which sends win32u down its offscreen path — the window still draws and still receives `WM_PAINT`, but never reaches a display. |
| `winewayland.drv` (default) | A floating window for any Windows program. For a bridged plug-in you get that floating window *in addition to* the copy in the FX window. Keep this if the prefix also runs ordinary Windows applications, since `null` leaves those with no window at all. |

```sh
wine reg add 'HKCU\Software\Wine\Drivers' /v Graphics /d null /f
```

vstbridge logs a warning naming the current driver whenever it is not `null`.

## Licensing and credit

Wine is **LGPL v2.1**; this port's patches are derived work under the same licence. The patchset
also carries the five upstream HaikuPorts patches, whose authorship (including X512's Haiku work)
is preserved in the `From:` lines of each patch. The recipe is derived from the haikuports
`app-emulation/wine` recipe.

The three Haiku fixes here are intended for upstream — if they land in haikuports, this
repository becomes unnecessary and `pkgman install wine` will be enough.
