"""Original synthetic mix fixture: keys, bass, melody and drums; no sampled recordings."""
from pathlib import Path
import numpy as np
import wave

out=Path(__file__).resolve().parents[1]/'tests/output'
out.mkdir(parents=True,exist_ok=True)
sr=48000
duration=16
mix=np.zeros((sr*duration,2),np.float64)
rng=np.random.default_rng(8503)
beat=60/112

def add(sound,at,pan=0,gain=1):
    start=int(at*sr)
    n=min(len(sound),len(mix)-start)
    if n<=0:return
    mix[start:start+n,0]+=sound[:n]*np.sqrt((1-pan)/2)*gain
    mix[start:start+n,1]+=sound[:n]*np.sqrt((1+pan)/2)*gain

def tone(note,length,kind):
    t=np.arange(int(sr*length))/sr
    f=440*2**((note-69)/12)
    if kind=='bass':
        y=np.sin(2*np.pi*f*t)+.26*np.sin(4*np.pi*f*t)+.07*np.sin(6*np.pi*f*t)
        env=(1-np.exp(-t/.007))*np.exp(-t/0.22)*np.minimum(1,(length-t)/.04)
    elif kind=='keys':
        y=sum((1/h**1.7)*np.sin(2*np.pi*f*h*t+(.12 if h>1 else 0)) for h in range(1,8))
        env=(1-np.exp(-t/.004))*np.exp(-t/.32)*np.minimum(1,(length-t)/.06)
    else:
        phase=2*np.pi*f*(t+.0015*np.sin(2*np.pi*5*t)/(2*np.pi*5))
        y=sum((.78**(h-1)/h)*np.sin(h*phase) for h in range(1,10))
        env=(1-np.exp(-t/.025))*np.minimum(1,(length-t)/.07)*(.9+.1*np.sin(2*np.pi*4.9*t))
    return y*env

chords=[[60,64,67,69],[57,60,64,67],[62,65,69,72],[59,62,65,69]]
roots=[36,33,38,43]
for bar in range(8):
    chord=chords[bar%4]
    for offset in [0,1.65,2.5,3.65]:
        at=(bar*4+offset)*beat
        for note in chord:add(tone(note,.8,'keys'),at,pan=-.6,gain=.032)
    for b in range(4):
        note=roots[bar%4]+[0,7,12,7][b]
        add(tone(note,.48,'bass'),(bar*4+b)*beat,pan=.08,gain=.17)
    for b in range(4):
        at=(bar*4+b)*beat
        t=np.arange(int(.25*sr))/sr
        kick=np.sin(2*np.pi*(48*t+3*(1-np.exp(-t*30))))*np.exp(-t*26)
        add(kick,at,pan=0,gain=.10 if b%2==0 else .04)
        if b%2:
            n=rng.normal(0,1,len(t));brush=np.convolve(n,[.25,.5,.25],mode='same')*np.exp(-t*20)*(1-np.exp(-t*1200))
            add(brush,at,pan=.32,gain=.045)
        for sub in [0,2/3]:
            t=np.arange(int(.09*sr))/sr;n=rng.normal(0,1,len(t));high=np.r_[0,np.diff(n)]
            add(high*np.exp(-t*70),(bar*4+b+sub)*beat,pan=.55,gain=.010)

melody=[(0,72,1),(1.5,76,.5),(2,79,.7),(3,76,.6),(4,74,.8),(5,72,.6),(6,69,.9),(7.3,72,.5),
        (8,74,.8),(9,77,.5),(10,81,.8),(11,79,.65),(12,77,.8),(13,74,.6),(14,71,.8),(15,67,.6),
        (16,72,1.4),(18,76,.8),(19,79,.7),(20,76,.8),(21,72,.6),(22,69,1.4),(24,74,.8),(25,77,.6),(26,79,.8),(27,77,.6),(28,74,.8)]
for at,note,length in melody:add(tone(note,length*beat,'lead'),at*beat,pan=.28,gain=.085)
mix*=min(1,.72/np.max(np.abs(mix)))
mix[:sr//20]*=np.linspace(0,1,sr//20)[:,None]
mix[-sr//2:]*=np.linspace(1,0,sr//2)[:,None]
with wave.open(str(out/'RadioRoom-original.wav'),'wb') as w:
    w.setnchannels(2);w.setsampwidth(2);w.setframerate(sr);w.writeframes((mix*32767).astype('<i2').tobytes())
print(out/'RadioRoom-original.wav')
