# reAAF

REAPER extension that imports AAF (Advanced Authoring Format) session files as REAPER projects, relying on [LibAAF](https://github.com/agfline/LibAAF).

[![Build](https://github.com/smandrap/reAAF/actions/workflows/build.yml/badge.svg)](https://github.com/smandrap/reAAF/actions/workflows/build.yml)
[![Tests](https://github.com/smandrap/reAAF/actions/workflows/tests.yml/badge.svg)](https://github.com/smandrap/reAAF/actions/workflows/tests.yml)
[![CodeQL](https://github.com/smandrap/reAAF/actions/workflows/github-code-scanning/codeql/badge.svg)](https://github.com/smandrap/reAAF/actions/workflows/github-code-scanning/codeql)

---

> **reAAF is read-only** : it imports AAF files into REAPER, it does not add capability to export them. It is not meant as a replacement for [AATranslator](https://www.aatranslator.com.au/). If you need advanced AAF support, or support for a wide range of other formats, you should buy AATranslator.

---

## Features

Most of what you expect from an AAF importer. In particular, most of what LibAAF exposes.

---

## AAF Format Support

AAF has inherent limitations regardless of the importer.
For more info, see [this document](docs/aaf-format-support.md).

BEFORE YOU REPORT A BUG, **READ THE DOCUMENT.**

---

## Usage

Drag/Drop AAF files in REAPER, or use File > Open dialog.

---

## Install

[Reapack](https://reapack.com) repository:
```
https://raw.githubusercontent.com/smandrap/reAAF/refs/heads/main/index.xml
```

---

## Dependencies

- [justinfrankel/reaper-sdk](https://github.com/justinfrankel/reaper-sdk) + [WDL](https://github.com/justinfrankel/WDL)
- [agfline/LibAAF](https://github.com/agfline/LibAAF) — included as git submodule

---

## Build

```bash
git clone --recurse-submodules https://github.com/smandrap/reAAF.git
```
clone reaper-sdk and WDL in reAAF/extern, then

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

On macOS the plugin is copied automatically to `~/Library/Application Support/REAPER/UserPlugins/`.  
On Windows it is copied to `%APPDATA%\REAPER\UserPlugins\`. Requires MSVC — MinGW is unsupported.  
On Linux copy `reaper_reAAF.so` to `~/.config/REAPER/UserPlugins/` manually.

---

## Thanks

This project wouldn't exist without [Adrien Gesta-Fline](https://github.com/agfline) and his work on [LibAAF](https://github.com/agfline/LibAAF). AAF is a genuinely painful format to deal with, and he did the hard work, I just made the REAPER bindings. If you find reAAF useful, go thank him, star his repo, and consider supporting his work.

---

## License

GPLv3+
