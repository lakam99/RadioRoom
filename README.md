# RadioRoom 0.1.1

A mix-bus VST3 effect and standalone player for a 1920s-to-1950s radio sound. A single shared room feeds stereo-linked compression and an oversampled tube/speaker path, helping separate recordings feel like one ensemble in one acoustic space.

## Start here

Put **one instance on the bus containing the entire mix**. Start with **1952 / Warm broadcast**. Raise **Room** toward 30–45% for more shared reflections; use **Distance** to move the common microphone back. Keep **Decay** around 0.35–0.7 seconds for a small studio rather than a reverberant hall. **Glue** gently connects the dynamics. **Width** at zero produces mono; 100% preserves the original stereo width. Match the final level with **Output**.

**1928 / Parlour wireless** is narrower, more focused and more coloured. **One room / Open ensemble** keeps more bandwidth and stereo while adding a common acoustic signature. These are creative era interpretations, not measured emulations of a specific radio model. A stereo mix processor cannot remove room reverberation already printed into individual recordings or reposition its individual instruments.

All fourteen controls and bypass are host-automatable. Drag a knob; double-click to reset. Hover for help. Every live value uses a cyan fluorescent-style display. Three raster artwork finishes are included: ivory/walnut, graphite and walnut. The standalone native window follows the OS light/dark theme.

| Control | Function |
|---|---|
| Era | Morph 1920–1959 tonal character |
| Bandwidth | Open or narrow bass and treble together |
| Tubes | 4x oversampled soft saturation, with drive compensation |
| Speaker | Cabinet resonance and upper-mid softening |
| Body / Presence | Broad low-mid / upper-mid EQ, ±6 dB |
| Glue | Stereo-linked soft-knee compression |
| Input | Gain into the common capture chain |
| Room | Common early reflections and dense eight-line room field |
| Decay | Room decay, 0.15–1.8 seconds |
| Distance | Direct/reflected balance and acoustic damping |
| Width | Mono to original stereo width |
| Mix | Latency-aligned dry / processed blend |
| Output | Processed output trim |

There is no added static, crackle, synthetic speech or always-on hiss. Silence stays silent until an existing room tail decays. Output is floating point and is not a brickwall limiter: high Input/Output/EQ settings can overload your next processor. Watch the output meter and reduce Output as needed. Dry and bypass retain original level and the fixed reported latency.

## Windows installation

Download the Windows x64 ZIP from [GitHub Releases](https://github.com/lakam99/RadioRoom/releases/latest). Extract it, open the RadioRoom folder, then run `Install-RadioRoom.cmd`.

- Standalone: `%LOCALAPPDATA%\Programs\RadioRoom\RadioRoom.exe`
- VST3: `%LOCALAPPDATA%\Programs\Common\VST3\RadioRoom.vst3`
- Start menu: RadioRoom

Restart or rescan your DAW's **effects** list. If required, add the user VST3 directory above to its scan paths. The installer never stops a DAW or other process. Close RadioRoom before updating it.

In standalone, **Open Mix** loads WAV, AIFF, FLAC or other JUCE-supported formats; the icon buttons rewind, play, stop and loop. The Audio button selects the output device. Processing settings and finish are saved between launches; the audio file is not embedded in settings. The VST receives live audio from the host and stores all processing settings in the song.

## Building and verification

C++20, CMake 3.22+, and a native toolchain are required. JUCE 9.0.6 is vendored, with its licensing retained. On Windows, install Visual Studio 2022 C++ Build Tools. `Build-source.ps1` configures and builds both targets.

```powershell
./Build-source.ps1
./scripts/Test-RadioRoom.ps1
```

CLI tools in the standalone executable:

```text
RadioRoom.exe --test dsp-report.json
RadioRoom.exe --host path/to/RadioRoom.vst3 --report host-report.json
RadioRoom.exe --snapshot panel.png --skin 0
RadioRoom.exe --process input.wav --output result.wav --preset 1
```

Offline WAV rendering uses preset indices 0 (1928), 1 (1952), 2 (open ensemble), compensates plugin latency, writes 24-bit stereo and retains a 2.5-second room tail. It refuses to overwrite its input. It does not render the current standalone settings. Mono files are duplicated to stereo; multichannel files are not a supported mixdown workflow.

Tests exercise 44.1/48/96 kHz, variable blocks including larger-than-advertised blocks, silence, extreme automation, exact delayed dry/bypass, mono layout, stereo symmetry, room decay, UI attachments and state recall. The host test loads the actual VST3, opens native editor peers and removes 24 instances across three cycles, then checks Windows module unload. This is independent-host verification, not certification of every DAW or an actual Studio Pro project. Windows x64 is the verified platform for this build.

## Artwork and licensing

Original source: AGPL-3.0-or-later; see LICENSE.txt. JUCE retains its original AGPL/commercial terms; see JUCE-LICENSE.md and third_party/JUCE/LICENSE.md. AI-generated raster artwork is documented in resources/generated/PROMPTS.md. Component atlases and two alternate finishes were reused from this workspace's NoiseBase/CrowdBase artwork. No SVG artwork is used. The optional demo is an original deterministic synthesised instrumental fixture, not a historical recording.

## Releases and automatic updates

The standalone checks GitHub for newer stable releases at startup, at most once per day. **Updates** offers a manual check and an automatic-check toggle. The VST Updates button opens the installed standalone updater. Network and installation work run outside the VST and audio callback. Download progress appears in the main fluorescent display. Download and installation require your confirmation. Save and close DAWs using RadioRoom before installing; the updater never ends a DAW process.

The updater validates repository/tag URLs, SHA-256 against both GitHub asset metadata and SHA256SUMS.txt, safe archive paths, package identity and every payload file. Installation is staged, retains the previous version outside the VST scan folder, and rolls back if either application or VST replacement fails. `Uninstall-RadioRoom.ps1` removes owned installs while retaining user settings. The initial local 0.1.0 prototype can be migrated only when its exact known executable and VST hashes match.

Pushes and pull requests build and verify Windows x64. Pushing a stable `vX.Y.Z` tag matching CMake creates a draft release, uploads and verifies asset sizes/digests, then publishes it as latest only after all checks pass. Secrets are scanned in source history and nested release archives. Dependencies and GitHub actions are pinned. CI has no publishing credentials beyond the per-job GitHub token. Source, Windows binaries and checksums ship with each release.

The demo melody, accompaniment and synthesis are original to `scripts/make-demo.py`; no sampled music or existing recording is used.
