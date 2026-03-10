# reaper_aaf

A native C++ REAPER extension that imports AAF (Advanced Authoring Format) session
files as REAPER projects.

Supported DAW sources: Avid Media Composer, ProTools, Adobe Premiere, Davinci Resolve,
Logic Pro, Fairlight — whatever LibAAF supports.

---

## Features

- Registers `.aaf` in REAPER's **File → Open** dialog filter
- Registers an action **"AAF: Import AAF session file"** in the Actions list
- Tracks: name, channel count (mono/stereo), volume, pan, mute, solo, colour
- Clips: position, length, source offset, clip gain, mute
- Fades: fade-in / fade-out length and curve shape
- Cross-fades: handled via overlapping clips + complementary fades
- Automation: track and clip volume/pan varying envelopes
- Markers and regions
- Embedded essences: automatically extracted to `<aafname>-media/` folder
- External essences: auto-located relative to the AAF file

---

## Dependencies

| Dependency | Location |
|---|---|
| [justinfrankel/reaper-sdk](https://github.com/justinfrankel/reaper-sdk) | `extern/reaper-sdk/` |
| [agfline/LibAAF](https://github.com/agfline/LibAAF) | `extern/LibAAF/` |

Set them up as submodules:

```bash
git submodule add https://github.com/justinfrankel/reaper-sdk.git extern/reaper-sdk
git submodule add https://github.com/agfline/LibAAF.git extern/LibAAF
git submodule update --init --recursive
```

---

## Building

### macOS (Apple Silicon + Intel universal binary)

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release
```

The compiled `reaper_aaf.dylib` is automatically copied to
`~/Library/Application Support/REAPER/UserPlugins/`.

### Windows (requires Visual Studio 2022 — MinGW is unsupported by REAPER SDK)

```powershell
mkdir build; cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

The compiled `reaper_aaf.dll` is automatically copied to
`%APPDATA%\REAPER\UserPlugins\`.

### Linux

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

Copy `reaper_aaf.so` to `~/.config/REAPER/UserPlugins/`.

---

## Project structure

```
reaper_aaf/
├── CMakeLists.txt
├── include/
│   └── aaf_import.h        ← public interface for the import layer
├── src/
│   ├── main.cpp            ← REAPER entry point, registration
│   └── aaf_import.cpp      ← AAF → RPP conversion (all the meat)
└── extern/
    ├── reaper-sdk/         ← git submodule
    └── LibAAF/             ← git submodule
```

---

## Architecture

```
REAPER "File > Open *.aaf"
       │
       ▼
projectimport hook (main.cpp)
       │  aaf_EnumFileExtensions() → tells REAPER to show .aaf in Open dialog
       │  aaf_ImportProject()      → called with the chosen file path
       ▼
ImportAAF() (aaf_import.cpp)
       │
       ├─ aafi_alloc() + aafi_load_file()    ← LibAAF parses the AAF
       │
       ├─ AAFI_foreachAudioTrack             ← walk tracks
       │     └─ AAFI_foreachTrackItem        ← walk clips on each track
       │           ├─ aafi_timelineItemToAudioClip()
       │           ├─ aafi_getFadeIn() / aafi_getFadeOut()
       │           └─ aafi_extract_audio_essence()  (if embedded)
       │
       └─ ProjectStateContext::AddLine()      ← emit RPP tokens to REAPER
```

---

## Known limitations / future work

- **Tempo map**: AAF carries timecode but not a tempo map. The project is
  imported at 120 BPM; adjust manually after import.
- **Video tracks**: LibAAF supports a single video clip. Not yet mapped.
- **Bus routing**: AAF routing groups not yet mapped to REAPER routing.
- **FX chains**: AAF plugin references are not mapped (no standard mapping).
- **Write support**: planned; will require a separate AAF writing layer.

---

## License

GPL-2.0-or-later (matching LibAAF's license requirement).
