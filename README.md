<p align="center">
  <img src="assets/icon_256.png" width="128" height="128" alt="KE-310 Organ Icon">
</p>

<h1 align="center">KE-310 Organ &amp; KE Rhythmbox</h1>
<p align="center">
  <strong>Retro Home Organ Collection #1</strong> by <strong>Yuguen Organ (ゆうげんオルガン)</strong><br>
  1979 Analog Transistor Home Organ &amp; Built-In / Standalone Analog Rhythm Synthesizer &mdash; VST3 / macOS AudioUnit (AU) / Standalone
</p>
<p align="center">
  <em>Current Status: Public Alpha (<code>v0.1.1-alpha</code>) &bull; License: GPLv3</em>
</p>

<p align="center">
  <img src="assets/gui_screenshot.png" alt="KE-310 Organ GUI with Embedded KE Rhythmbox">
</p>

<details>
<summary>Standalone <strong>KE Rhythmbox</strong> GUI &amp; <strong>KE-310 Organ</strong> Settings View Screenshots / 単体版リズムボックス＆設定画面スクリーンショット</summary>

<p align="center">
  <strong>KE Rhythmbox (Standalone / VST3 / AU)</strong><br>
  <img src="assets/gui_rhythmbox_screenshot.png" alt="KE Rhythmbox Standalone GUI">
</p>

<p align="center">
  <strong>KE-310 Organ &mdash; Settings View (Upper knobs &amp; tone levers stay visible)</strong><br>
  <img src="assets/gui_settings_screenshot.png" alt="KE-310 Organ Settings View">
</p>

</details>

---

## Overview / 概要

**KE-310 Organ** is the first instrument in the **Retro Home Organ Collection** developed by **Yuguen Organ (ゆうげんオルガン)**.
It models the tone generators, divider architecture, analog formant filters, keying/envelope buses, spring reverb, built-in hornless rotary drum speaker, and built-in **KE Rhythmbox** analog percussion synthesizer of the 1979 KE-310 transistor home organ in real-time C++20 DSP (`JUCE 8`). Alongside **KE-310 Organ**, the rhythm section is also included as a dedicated standalone instrument plugin (**KE Rhythmbox**).

**KE-310 Organ** は、**Yuguen Organ（ゆうげんオルガン）** が贈る **Retro Home Organ Collection 第1弾** です。
1979年製トランジスタ電子オルガン（KE-310）の音源・分周器・フォルマントフィルタ・キーイング回路・スプリングリバーブ・内蔵ホーンなしロータリードラムスピーカーに加え、実機内蔵のリズムマシン **KE Rhythmbox** までをリアルタイム C++20 DSP（JUCE 8）で再現しました。オルガン本体への内蔵に加え、**KE Rhythmbox** 単体プラグイン（VST3 / AU / Standalone）も同梱しています。

> [!NOTE]
> This is an **Alpha release (`v0.1.1-alpha`)**. Feedback, bug reports, and DAW compatibility reports via GitHub Issues are welcome.

---

## Key Features / 主な機能

### KE-310 Organ
- **3-Manual Organ Architecture (49 / 49 / 13 Keys)**
  - **Upper Manual (49 keys, C3–C7)**: 10 tone levers (`TIBIA 16'/8'/4'/2 2/3'`, `DIAPASON 8'`, `STRING 16'/8'/4'`, `OBOE 8'`, `CLARINET 8'`) + `UPPER SUSTAIN`
  - **Lower Manual (49 keys, C2–C6)**: 6 tone levers (`TIBIA 8'/4'`, `DIAPASON 8'`, `STRING 8'/4'`, `HORN 8'`)
  - **Pedalboard (13 keys, C2–C3)**: 3 tone levers (`BOURDON 16'`, `MAJOR FLUTE 8'`, `STRING BASS`), authentic low-note priority monophonic VCO + `PEDAL SUSTAIN`
- **Upper Preset Section**:
  - `ORGAN SILENT`, `PERCUS. 2 2/3'` (single-trigger legato behavior), `WOW TRUMPET` (envelope-triggered wah sweep with legato carry-over), `BANJO`, `MANDOLIN`, `MARIMBA` (alternating 8'/4' repeat VCA), `PIANO`, `HARPSICHORD`
  - Authentic shared 8'/4' decay keying behavior (`PIANO` / `HARPSICHORD` + `UPPER SUSTAIN` interaction matching hardware circuit behavior)
- **Built-in Hornless Rotary Drum Speaker & Effects**:
  - Mechanical **single-rotor drum model** with smooth asymmetric motor acceleration/deceleration inertia: `CHORUS` (Slow), `TREMOLO` (Fast), and Brake (both OFF)
  - User-adjustable **Rotary Slow (Hz)** and **Rotary Fast (Hz)** speeds
  - `VIBRATO`, `VIBRATO FAST`, `DELAY VIBRATO` (disabled automatically when `PIANO` / `HARPSICHORD` active, just like hardware), `ENSEMBLE` (routes Stationary voices into Rotor), `REVERB` (spring tank model), and loudness-compensated `EXPRESSION` pedal
- **4-Stereo-Bus Multi-Out (DAW Flexibility)**:
  - `Main Mix` (Stereo) + `Rotor Out` (Stereo) + `Stationary Out` (Stereo) + `Rhythm Out` (Stereo)
  - When both `CHORUS` and `TREMOLO` are OFF, `Rotor Out` passes completely dry (bit-exact bypass) so DAW users can freely insert external rotary speaker or guitar/organ amp plugins per bus
  - Configurable rhythm bus routing in Settings: `Stationary (Bypass Ensemble)` (default — sums post-ensemble so `ENSEMBLE ON` never rotates drums), `Rhythm Out (+ Main)`, or `Rhythm Out (Isolated)`
- **Clean Layout & Gear Settings View**:
  - All 10 rotary knobs and 35 tone levers remain visible at all times; clicking the top-right **Gear icon** toggles the lower bay between the embedded **KE Rhythmbox** deck and **Settings** (`MIDI Routing & Manual Assignment`, `Tuning, Expression & Rotary`, `Rhythmbox Settings`)
- **28 Built-in Registration Presets & Flexible MIDI Routing**:
  - Multi-Channel mode (`Upper / Lower / Pedal` independently assignable to `Omni / Ch 1..16 / Off`) and Single-Keyboard Split mode
  - Octave shifts (`-2..+2`), optional pedal octave folding, physical key range clamp toggle, MIDI `CC#11` Expression, `CC#7` Volume, and `CC#64` Knee Sustainer

### KE Rhythmbox (Embedded & Standalone)
- **11 Analog Percussion Voices & 16 Hardware Patterns**:
  - Real-time synthesis of Bass Drum, Snare Drum, Cymbal, High-Hat, Tambourine, Low/High Congas, Low Bongo, Claves, Rim Shot, and Cowbell (no external samples)
  - 16 ROM rhythm patterns (`WALTZ`, `BALLAD`, `MARCH`, `TANGO`, `ROCK I–III`, `BOSSANOVA`, `SWING I–III`, `SAMBA`, `MAMBO`, `BEGUINE`, `CHA-CHA`, `RHUMBA`), `Multi-Pattern` Wired-OR combinator (checkbox or multi-touch simultaneous press), and 0-selection idle state
- **Hardware Action Controls & Organ Synchro-Start (`KE-310 Organ`)**:
  - **`RHYTHM`** (Red button): Arms synchro-start from Beat 1 whenever Pedal or Lower Manual (`C2–C5`) is played; turning OFF stops immediately
  - **`BREAK`** (White button): When `RHYTHM` is ON, plays from Beat 1 only while Pedal or Lower Manual (`C2–C5`) keys are held and stops immediately on release
  - **`VARIATION`** (Black button): Enables pattern variation on supported rhythms (all patterns except `TANGO`, `BEGUINE`, `RHUMBA` per manual)
- **Tone, Tempo Sync & GM MIDI Triggering**:
  - `TONE` (Drums &larr; Flat &rarr; Cymbals tilt/balance), `VOLUME`, `TEMPO` (40–240 BPM) + external mode readout (`HOST TEMPO` / `HOST SYNC`), pinkish-red flashing **Tempo Lamp**, and `Analog Drift`
  - Sync modes: `Internal`, `Host Tempo` (follows DAW BPM with `RHYTHM`/`BREAK` or `START/STOP`), and `Host Sync` (follows DAW transport & PPQ)
  - GM Drum Map MIDI triggering (`Omni / Ch 1..16 / Off`; defaults to `Ch 10` inside `KE-310 Organ` and `Omni` in standalone `KE Rhythmbox`, plus 11 audition trigger pads in standalone)

---

## Download & Installation / ダウンロードと導入方法

Pre-built binaries for both **KE-310 Organ** and **KE Rhythmbox** are published on the [GitHub Releases](../../releases) page (`.zip` archives for **Windows x64**, **macOS Universal arm64+x86_64**, and **Linux x64**).

### Windows (x64)
- **VST3**: Copy `KE-310 Organ.vst3` and/or `KE Rhythmbox.vst3` into `C:\Program Files\Common Files\VST3\`
- **Standalone**: Run `KE-310 Organ.exe` and/or `KE Rhythmbox.exe` directly from any folder
- **Audio Driver Note**: The Standalone apps use **Windows Audio (WASAPI Shared / Exclusive / Low Latency)** and **DirectSound** (proprietary ASIO drivers are not bundled). If you prefer using an ASIO audio interface directly, please load the `.vst3` plugins inside an ASIO-compatible DAW or VST3 host.

### macOS (Apple Silicon + Intel Universal)
- **VST3**: Copy `KE-310 Organ.vst3` and/or `KE Rhythmbox.vst3` to `/Library/Audio/Plug-Ins/VST3/`
- **AudioUnit (AU)**: Copy `KE-310 Organ.component` and/or `KE Rhythmbox.component` to `/Library/Audio/Plug-Ins/Components/`
- **Standalone**: Copy `KE-310 Organ.app` and/or `KE Rhythmbox.app` to `/Applications/`

> [!IMPORTANT]
> **macOS Gatekeeper Notice (Alpha Unsigned Builds)**
> Because alpha builds are not yet Apple-notarized, macOS may show a *"cannot be opened / damaged"* warning on first launch. After copying the files, clear the quarantine attribute in Terminal:
> ```bash
> xattr -cr "/Library/Audio/Plug-Ins/VST3/KE-310 Organ.vst3"
> xattr -cr "/Library/Audio/Plug-Ins/Components/KE-310 Organ.component"
> xattr -cr "/Applications/KE-310 Organ.app"
>
> xattr -cr "/Library/Audio/Plug-Ins/VST3/KE Rhythmbox.vst3"
> xattr -cr "/Library/Audio/Plug-Ins/Components/KE Rhythmbox.component"
> xattr -cr "/Applications/KE Rhythmbox.app"
> ```

### Linux (x64)
- **VST3**: Copy `KE-310 Organ.vst3` and/or `KE Rhythmbox.vst3` to `~/.vst3/`
- **Standalone**: Make `KE-310 Organ` / `KE Rhythmbox` executable (`chmod +x "KE-310 Organ" "KE Rhythmbox"`) and run them

---

## Build from Source / ソースからのビルド

Requirements: CMake >= 3.22, a C++20 compiler, Git, and internet access (to fetch JUCE 8.0.6 and Catch2 v3.7.1 via `FetchContent`).

```bash
# Configure & Build (Release VST3 / AU / Standalone / Catch2 Tests for both plugins)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run all 34 Catch2 DSP & Plugin unit tests
ctest --test-dir build --output-on-failure -V
```

*(On Linux, pass `-G Ninja` if using Ninja; on macOS Universal builds, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"`.)*

---

## License & Disclaimer / ライセンス・免責事項

- **License**: Released under the **GNU General Public License v3.0 (GPLv3)** (compatible with JUCE 8 open-source licensing). The copyright holder (**Yuguen Organ**) retains the right to distribute this software under separate commercial or proprietary license terms. By submitting a Pull Request or contribution to this repository, you agree that your contribution may be licensed and distributed under both GPLv3 and separate commercial terms; see [LICENSE](LICENSE) for full details.
- **Framework**: Built with [JUCE 8](https://juce.com/).
- **Disclaimer**: *KE-310 Organ* and *KE Rhythmbox* are independent software modeling projects created by **Yuguen Organ (ゆうげんオルガン)**. They are not affiliated with, endorsed by, or sponsored by any hardware musical instrument manufacturer. All stop and rhythm names refer to traditional electronic organ terminology.
