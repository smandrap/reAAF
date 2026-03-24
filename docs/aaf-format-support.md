Conversion fidelity depends on what the source application exported. AAF has inherent limitations regardless of the importer.

### What AAF Does NOT Include

- **MIDI Data:** audio/video* only; MIDI cannot be transferred
- **Plugins/VST/AU:** forget it
- **Tempo Map:** nope
- **Time Signature:** not supported
- **Pan Automation:** unreliable
- **Markers:** inconsistent
- **Track Colors:** rarely (or never) preserved
- **Sends/Routing:** awol

### What AAF DOES Include

- **Audio Files:** yes
- **Track Name/Order:** yes 
- **Clip Positions:** yes, precise
- **Clip Gain:** less solid, but it's mostly there 
- **Volume Automation:** rare
- **Fades/Crossfades:** supported, some daws don't export it (Logic)
- **Static Pan:** inconsistent

---

## DAW & NLE-Specific Notes

### Pro Tools

- Track faders export only when "Enforce Media Composer Compatibility" is **disabled**
- Clip gain exports only when "Enforce Media Composer Compatibility" is **enabled**
- Clip gain values exceeding +12dB are not exported
- Session markers are excluded from AAF exports
- Pan automation ok most of the times, but not always

### DaVinci Resolve

- No volume automation
- Static track fader levels are not exported
- Pan settings are not included

### Logic Pro (Apple)

- No fade and crossfade information 
- Static fader positions are not exported
- Markers and locators are not included

### Sequoia/Samplitude (MAGIX)

- Mono AAF exports are not supported; stereo interleaved format is required
- Split stereo panning mode is not exported

### Nuendo/Cubase (Steinberg)

- Static track fader positions are not exported
- Pan positions and pan automation are not included
- Markers and locators are excluded
- Stereo tracks export as consecutive mono channels with identical names

### Premiere Pro (Adobe)

- Custom fade curves (exponential, S-curve) converted to linear fades
- Static track fader positions are not exported
- Pan positions are not included

---

Source: https://abletonliveaaf.shop/help/index.html