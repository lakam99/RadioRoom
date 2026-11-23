"""Expand the approved RadioRoom instrumental using its original synthesis voices.

No sampled music, generated lyrics, or external audio services are used.
The original 16-second demo and the plugin binaries are left unchanged.
"""
from pathlib import Path
from functools import lru_cache
import argparse
import json
import wave
import numpy as np

parser = argparse.ArgumentParser()
parser.add_argument('--out', type=Path, required=True)
args = parser.parse_args()
out = args.out.resolve()
out.mkdir(parents=True, exist_ok=True)
sr = 48000
beat = 60 / 112
bars = 80
rng = np.random.default_rng(8503)

# Preserve the original tempo throughout the song, relaxing only the last two bars.
beat_lengths = np.full(bars * 4 + 8, beat)
beat_lengths[312:320] *= np.linspace(1, 112 / 90, 8)
beat_lengths[320:] = beat_lengths[319]
beat_times = np.r_[0, np.cumsum(beat_lengths)]

def at_time(at):
    return float(np.interp(at, np.arange(len(beat_times)), beat_times))

duration = at_time(bars * 4) + 3.0
mix = np.zeros((int(np.ceil(sr * duration)), 2), np.float64)
event_counts = {'keys': 0, 'bass': 0, 'lead': 0, 'drums': 0}

def add(sound, at, pan=0, gain=1, jitter=0):
    start = int((at_time(at) + jitter) * sr)
    if start < 0:
        sound = sound[-start:]
        start = 0
    n = min(len(sound), len(mix) - start)
    if n <= 0:
        return
    mix[start:start+n, 0] += sound[:n] * np.sqrt((1-pan)/2) * gain
    mix[start:start+n, 1] += sound[:n] * np.sqrt((1+pan)/2) * gain

@lru_cache(maxsize=256)
def tone(note, length, kind):
    # Same waveform and envelope definitions as the accepted 16-second demo.
    t = np.arange(int(sr * length)) / sr
    f = 440 * 2 ** ((note-69)/12)
    if kind == 'bass':
        y = np.sin(2*np.pi*f*t) + .26*np.sin(4*np.pi*f*t) + .07*np.sin(6*np.pi*f*t)
        env = (1-np.exp(-t/.007)) * np.exp(-t/.22) * np.minimum(1, (length-t)/.04)
    elif kind == 'keys':
        y = sum((1/h**1.7)*np.sin(2*np.pi*f*h*t+(.12 if h > 1 else 0)) for h in range(1, 8))
        env = (1-np.exp(-t/.004)) * np.exp(-t/.32) * np.minimum(1, (length-t)/.06)
    else:
        phase = 2*np.pi*f*(t+.0015*np.sin(2*np.pi*5*t)/(2*np.pi*5))
        y = sum((.78**(h-1)/h)*np.sin(h*phase) for h in range(1, 10))
        env = (1-np.exp(-t/.025)) * np.minimum(1, (length-t)/.07) * (.9+.1*np.sin(2*np.pi*4.9*t))
    result = y * env
    result.setflags(write=False)
    return result

def note(note_number, at, length, kind='lead', gain=.085, pan=.28, loose=False):
    seconds = round(at_time(at+length)-at_time(at), 4)
    sound = tone(note_number, seconds, kind)
    add(sound, at, pan=pan, gain=gain, jitter=float(rng.uniform(-.004, .006)) if loose else 0)
    event_counts[kind] += 1

chords = {
    'C6': ([60,64,67,69],36), 'Am7': ([57,60,64,67],33),
    'Dm7': ([62,65,69,72],38), 'G9': ([59,62,65,69],43),
    'F6': ([57,60,62,65],41), 'Fm6': ([56,60,62,65],41),
    'A7': ([57,61,64,67],33), 'Em7': ([55,59,62,64],40),
    'E7': ([56,59,62,64],40), 'G7': ([55,59,62,65],43),
}
A = [['C6'],['Am7'],['Dm7'],['G9'],['C6'],['Am7'],['Dm7'],['G9']]
B = [['F6'],['Fm6'],['C6'],['A7'],['Dm7'],['G9'],['Em7','A7'],['Dm7','G9']]
sections = [
    ('Intro',0,4,A[:4],.70),
    ('Original theme',4,12,A,1.0),
    ('Theme answer',12,20,A,.98),
    ('Bridge',20,28,B,1.02),
    ('Theme return',28,36,A,1.04),
    ('Keys solo',36,44,A,.97),
    ('Keys solo / bridge',44,52,B,1.02),
    ('Quiet passage',52,60,A,.70),
    ('Final theme',60,68,A,1.06),
    ('Final answer',68,76,A,1.10),
    ('Ending',76,80,[['F6'],['Fm6'],['Dm7','G9'],['C6']],.90),
]
progression = [None] * bars
section_for_bar = [None] * bars
for name, start, end, changes, energy in sections:
    for i in range(start, end):
        progression[i] = changes[i-start]
        section_for_bar[i] = (name, energy)

def chord_at(bar, offset):
    choices = progression[bar]
    return choices[0] if len(choices) == 1 or offset < 2 else choices[1]

for bar in range(bars):
    section, energy = section_for_bar[bar]
    quiet = section == 'Quiet passage'
    solo = 'Keys solo' in section
    original = 4 <= bar < 12
    ending = bar >= 76
    comp_offsets = [0,1.65,2.5,3.65]
    if bar == 0:
        comp_offsets = [0,2.5]
    elif quiet:
        comp_offsets = [0,2.5] if bar < 56 else [0,1.65,3.65]
    elif solo:
        comp_offsets = [0,2.5,3.65] if bar % 2 else [.65,2,3.65]
    elif not original and bar % 4 == 3:
        comp_offsets = [0,1.65,2.5]
    if bar == 79:
        comp_offsets = [0,1.5]

    for offset in comp_offsets:
        voicing, _ = chords[chord_at(bar, offset)]
        voicing = list(voicing)
        # Sparse inversions in later sections keep the accompaniment moving.
        if not original and (bar // 4) % 3 == 1 and not ending:
            voicing = voicing[1:] + [voicing[0]+12]
        velocity = 1 if original else float(rng.uniform(.90,1.07))
        gain = .032*energy*(.76 if solo else 1)*velocity
        for n in voicing:
            note(n,bar*4+offset,1.2 if bar == 79 else .8/beat,'keys',gain,-.6,not original)

    for b in range(4):
        if bar == 79 and b > 1:
            continue
        _, root = chords[chord_at(bar,b)]
        pitch = root + [0,7,12,7][b]
        if not original and b == 3 and bar+1 < bars and (solo or bar % 4 == 3):
            next_root = chords[chord_at(bar+1,0)][1]
            pitch = next_root - 1 if next_root >= root else next_root + 1
        if quiet and b % 2:
            continue
        gain = .17*energy*(1 if original else float(rng.uniform(.93,1.04)))
        note(pitch,bar*4+b,.48/beat,'bass',gain,.08,not original)

    for b in range(4):
        if bar == 79 and b >= 2:
            continue
        at = bar*4+b
        t = np.arange(int(.25*sr))/sr
        kick = np.sin(2*np.pi*(48*t+3*(1-np.exp(-t*30))))*np.exp(-t*26)
        if not quiet or b % 2 == 0:
            add(kick,at,0,(.10 if b % 2 == 0 else .04)*energy)
            event_counts['drums'] += 1
        if b % 2 and bar > 0:
            n = rng.normal(0,1,len(t))
            brush = np.convolve(n,[.25,.5,.25],mode='same')*np.exp(-t*20)*(1-np.exp(-t*1200))
            add(brush,at,.32,.045*energy*(.6 if quiet else 1)*float(rng.uniform(.94,1.04)))
            event_counts['drums'] += 1
        for sub in [0,2/3]:
            if bar == 79 and (b > 0 or sub > 0):
                continue
            if quiet and sub > 0 and bar < 56:
                continue
            t = np.arange(int(.09*sr))/sr
            n = rng.normal(0,1,len(t))
            high = np.r_[0,np.diff(n)]
            add(high*np.exp(-t*70),at+sub,.55,.010*energy*(.7 if quiet else 1)*float(rng.uniform(.85,1.08)))
            event_counts['drums'] += 1

    # A light brush pickup into larger sections, no new percussion timbre.
    if bar in [11,19,27,35,43,51,59,67,75]:
        for offset,gain in [(3.3,.018),(3.66,.025)]:
            t = np.arange(int(.13*sr))/sr
            n = rng.normal(0,1,len(t))
            brush = np.convolve(n,[.25,.5,.25],mode='same')*np.exp(-t*28)*(1-np.exp(-t*1000))
            add(brush,bar*4+offset,.32,gain*energy)
            event_counts['drums'] += 1

# The first eight-bar melody is retained, rather than replaced by a new tune.
theme = [
 (0,72,1),(1.5,76,.5),(2,79,.7),(3,76,.6),(4,74,.8),(5,72,.6),(6,69,.9),(7.3,72,.5),
 (8,74,.8),(9,77,.5),(10,81,.8),(11,79,.65),(12,77,.8),(13,74,.6),(14,71,.8),(15,67,.6),
 (16,72,1.4),(18,76,.8),(19,79,.7),(20,76,.8),(21,72,.6),(22,69,1.4),(24,74,.8),
 (25,77,.6),(26,79,.8),(27,77,.6),(28,74,.8),(29,71,.6),(30,67,.8)]
answer = [
 (0,72,.8),(1.5,76,.5),(2.2,79,.6),(3.3,81,.5),(4,79,.9),(5.5,76,.6),(6.5,72,.9),
 (8,74,.7),(9,77,.5),(9.66,81,.65),(11,79,.7),(12,77,.65),(13,74,.6),(14,71,.7),(15,69,.5),
 (16,67,.6),(17,72,.6),(18,76,.8),(19.3,79,.5),(20,76,.8),(21,72,.7),(22,69,1.2),
 (24,74,.7),(25,77,.5),(26,76,.6),(27,74,.6),(28,71,.8),(29.5,67,.6),(30.5,72,.8)]
bridge = [
 (0,77,1.2),(1.66,81,.6),(2.66,79,.7),(4,80,1.1),(5.66,79,.6),(6.66,77,.8),
 (8,76,1.3),(10,79,.7),(11,76,.6),(12,73,.8),(13,76,.6),(14,79,1.1),
 (16,77,1.1),(17.66,76,.6),(18.66,74,.9),(20,71,1),(21.66,74,.6),(22.66,77,.8),
 (24,76,.8),(25,79,.6),(26,73,.8),(27,76,.6),(28,74,.8),(29,77,.5),(30,71,.9)]

def melody(events, start_bar, intensity=1, variation=False):
    for index,(at,n,length) in enumerate(events):
        vel = 1 if not variation else float(rng.uniform(.92,1.05))
        # Let phrases breathe at the ends of four-bar groups.
        vel *= .93 if index and length > 1 else 1
        note(n,start_bar*4+at,length,'lead',.085*intensity*vel,.28,variation)

# Intro pickup, then theme / answer / bridge / return.
melody([(13.66,67,.55),(14.66,71,.55)],0,.70)
melody(theme,4)
melody(answer,12,.98,True)
melody(bridge,20,1.0,True)
melody(theme,28,1.02,True)

# Composed piano solo: deliberate phrases with rests, arpeggios and resolutions.
# Each list occupies two bars; it uses the same keys voice as the accompaniment.
solo_phrases = [
 [(0,76,.45),(.66,79,.45),(1.66,81,.6),(2.66,79,.45),(3.5,76,.45),(4.66,72,.6),(5.66,76,.45),(6.66,79,.8)],
 [(0,77,.6),(1,81,.4),(1.66,84,.6),(2.66,81,.5),(4,79,.6),(5,77,.4),(5.66,74,.5),(6.66,71,.8)],
 [(.66,72,.45),(1.66,76,.5),(2.66,79,.6),(4,81,.6),(5,79,.45),(5.66,76,.5),(6.66,72,.8)],
 [(0,74,.6),(1,77,.4),(1.66,81,.5),(3,79,.5),(4,77,.5),(5,74,.4),(5.66,71,.5),(6.66,67,.8)],
 [(0,77,.6),(1,81,.4),(1.66,84,.7),(3,81,.45),(4.66,80,.7),(5.66,79,.5),(6.66,77,.8)],
 [(.66,76,.5),(1.66,79,.5),(2.66,83,.8),(4,85,.5),(5,81,.4),(5.66,79,.6),(6.66,76,.7)],
 [(0,77,.6),(1,81,.4),(1.66,84,.7),(3,81,.5),(4,79,.6),(5,77,.45),(5.66,74,.5),(6.66,71,.8)],
 [(0,76,.6),(1,79,.4),(1.66,83,.5),(2.66,85,.5),(3.66,81,.6),(4.66,77,.6),(5.66,74,.4),(6.66,71,.8)],
]
for phrase_index, phrase in enumerate(solo_phrases):
    for at,n,length in phrase:
        note(n,36*4+phrase_index*8+at,length,'keys',.12*float(rng.uniform(.94,1.07)),.05,True)

# Gentle horn responses late in the solo, then a thinner eight-bar passage.
for start,n in [(44,69),(46,67),(48,69),(50,67)]:
    melody([(6,n,.8),(7.3,n+3,.45)],start,.48,True)
melody([(0,72,1.6),(4,69,1.2),(8,74,1.4),(12,71,1.0)],52,.58,True)
melody([(0,72,1.4),(4,76,1),(8,77,.8),(9.66,76,.6),(10.66,74,.8),(12,71,.8),(13.66,67,.6),(14.66,71,.6)],56,.73,True)

# Final head keeps the familiar hook; the answer becomes a small closing flourish.
melody(theme,60,1.04,True)
final_answer = list(answer)
final_answer[-4:] = [(28,71,.6),(29,74,.5),(29.66,77,.5),(30.66,79,.8)]
melody(final_answer,68,1.08,True)
melody([
 (0,81,.7),(1,79,.6),(2,77,.7),(3,76,.65),
 (4,80,.8),(5.3,79,.6),(6.3,77,.8),
 (8,74,.7),(9,77,.5),(9.66,76,.5),(10.66,74,.6),(11.5,71,.5),
 (12,76,.6),(12.75,74,.45),(13.5,72,2.4)],76,.92,True)

# Fade only the outer edges, leaving a musical ending rather than fading a loop.
mix[:sr//20] *= np.linspace(0,1,sr//20)[:,None]
mix[-sr:] *= np.linspace(1,0,sr)[:,None]
if not np.isfinite(mix).all():
    raise RuntimeError('Non-finite audio')
raw_peak = float(np.max(np.abs(mix)))
active = mix[:int(at_time(bars*4)*sr)]
raw_rms = float(np.sqrt(np.mean(active**2)))
gain = min(.84/max(raw_peak,1e-10), .075/max(raw_rms,1e-10))
mix *= gain
peak = float(np.max(np.abs(mix)))
if peak >= 1:
    raise RuntimeError('Clipping')

# Stream conversion in chunks instead of allocating a second complete 24-bit file.
destination = out/'RadioRoom-After-Hours.wav'
with wave.open(str(destination),'wb') as w:
    w.setnchannels(2)
    w.setsampwidth(3)
    w.setframerate(sr)
    for start in range(0,len(mix),sr*4):
        pcm = np.rint(mix[start:start+sr*4]*8388607).astype(np.int32).reshape(-1)
        packed = np.empty((len(pcm),3),np.uint8)
        packed[:,0] = pcm & 255
        packed[:,1] = (pcm >> 8) & 255
        packed[:,2] = (pcm >> 16) & 255
        w.writeframesraw(packed.tobytes())

report = {
    'title':'After Hours', 'sample_rate':sr, 'bits':24, 'channels':2,
    'seconds':len(mix)/sr, 'tempo_bpm':112,
    'ending':'Last two bars relax toward 90 BPM; composed tonic ending.',
    'bars':bars, 'peak_dbfs':float(20*np.log10(max(peak,1e-10))),
    'active_rms_dbfs':float(20*np.log10(max(raw_rms*gain,1e-10))),
    'master_gain_db':float(20*np.log10(gain)), 'event_counts':event_counts,
    'finite':True, 'clipped_samples':int(np.count_nonzero(np.abs(mix)>=1)),
    'sections':[{'name':n,'start_bar':s,'end_bar':e,'start_seconds':round(at_time(s*4),3),'end_seconds':round(at_time(e*4),3)} for n,s,e,_,_ in sections],
    'synthesis':'Original demo voice equations retained; no sampled recordings or external music models.',
    'original_demo_unchanged':True,
}
(out/'arrangement-and-render.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print(json.dumps(report,indent=2))
print(destination)
