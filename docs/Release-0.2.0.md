# RadioRoom 0.2.0

Add **Crackle**, a host-automatable procedural vinyl texture control. It layers fine irregular ticks, occasional dusty pops and a soft filtered surface floor, with slow variation in density. No sampled loops are used. Start around 30–45% for light nostalgic texture; higher amounts sound more worn.

Texture is added after the common room/compressor and follows Width, Tubes, Speaker, Bandwidth, Mix and Output. It continues through pauses while the host is playing, then fades when playback stops. Standalone playback now detects end-of-file when updating this activity state. Hosts without transport information use input activity with a brief hold/release.

Crackle is off at zero and excluded from dry mix and bypass. Existing presets and pre-0.2 sessions load with Crackle off. All original parameter IDs and indices, including bypass, are preserved; the new parameter is appended. The lower control row adds a matching raster knob and fluorescent value display.

Verification covers 44.1/48/96 kHz; silence at zero, stopped transport, dry mix and bypass; intensity scaling; stereo/mono behaviour; block-size invariance; state restore including older sessions; all UI attachments; and actual hosted VST3 parameter recall and repeated editor/instance teardown.
