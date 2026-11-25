"""Compare identical music passages with procedural vinyl wear at 0/40/80%."""
from pathlib import Path
import json,wave
import numpy as np

root=Path(__file__).resolve().parents[1]
out=root/'tests/output'
pieces=[];report=[]
for amount in ['0','0.4','0.8']:
    with wave.open(str(out/f'RadioRoom-crackle-{amount}.wav'),'rb') as w:
        sr=w.getframerate();assert w.getsampwidth()==3 and w.getnchannels()==2
        raw=np.frombuffer(w.readframes(w.getnframes()),dtype=np.uint8).reshape(-1,3).astype(np.int32)
    signed=raw[:,0]|(raw[:,1]<<8)|(raw[:,2]<<16)
    audio=np.where(signed&0x800000,signed-0x1000000,signed).reshape(-1,2)/8388608
    assert np.isfinite(audio).all() and np.max(np.abs(audio))<.99999
    clip=audio[:sr*6].copy()
    ramp=sr//100;clip[:ramp]*=np.linspace(0,1,ramp)[:,None];clip[-ramp:]*=np.linspace(1,0,ramp)[:,None]
    pieces.extend([clip,np.zeros((int(sr*.3),2))])
    report.append({'crackle':float(amount),'start_seconds':(len(report)*6.3),'peak_dbfs':float(20*np.log10(np.max(np.abs(audio))))})
comparison=np.concatenate(pieces)
with wave.open(str(out/'RadioRoom-crackle-comparison.wav'),'wb') as w:
    w.setnchannels(2);w.setsampwidth(2);w.setframerate(sr);w.writeframes((comparison*32767).astype('<i2').tobytes())
(root/'tests/crackle-preview-report.json').write_text(json.dumps({'order':report,'note':'Same six-second music passage at original relative levels: off, 40%, 80%. No sampled vinyl or looped audio texture.'},indent=2))
print(json.dumps(report,indent=2))
