from pathlib import Path
import json,wave
import numpy as np

root=Path(__file__).resolve().parents[1]
out=root/'tests/output'

def read(file):
    with wave.open(str(file),'rb') as w:
        rate=w.getframerate();channels=w.getnchannels();width=w.getsampwidth();raw=w.readframes(w.getnframes())
    if width==2:y=np.frombuffer(raw,dtype='<i2').astype(float)/32768
    elif width==3:
        b=np.frombuffer(raw,dtype=np.uint8).reshape(-1,3).astype(np.int32)
        v=b[:,0]|(b[:,1]<<8)|(b[:,2]<<16)
        v=np.where(v&0x800000,v-0x1000000,v)
        y=v/8388608
    else:raise ValueError('Unsupported fixture encoding')
    return rate,y.reshape(-1,channels)

parts=[];metrics={};timeline=[];clock=0
for name,file in [('Original','RadioRoom-original.wav'),('One room / Open ensemble','RadioRoom-preset-2.wav'),('1952 / Warm broadcast','RadioRoom-preset-1.wav'),('1928 / Parlour wireless','RadioRoom-preset-0.wav')]:
    sr,y=read(out/file)
    peak=float(np.max(np.abs(y)));rms=float(np.sqrt(np.mean(y[:16*sr]**2)))
    metrics[name]={'peak_dbfs':float(20*np.log10(max(peak,1e-10))),'rms_dbfs':float(20*np.log10(max(rms,1e-10))),'clipped_samples':int(np.sum(np.abs(y)>=.99999)),'seconds':len(y)/sr}
    if metrics[name]['clipped_samples']:raise RuntimeError('Clipping in preview')
    clip=y[:sr*6].copy();clip*=.095/max(1e-8,float(np.sqrt(np.mean(clip**2))))
    fade=sr//100;clip[:fade]*=np.linspace(0,1,fade)[:,None];clip[-fade:]*=np.linspace(1,0,fade)[:,None]
    timeline.append({'section':name,'start_seconds':round(clock,2),'end_seconds':round(clock+6,2)})
    parts.extend([clip,np.zeros((int(.3*sr),2))]);clock+=6.3
comparison=np.concatenate(parts)
if np.max(np.abs(comparison))>=1:raise RuntimeError('Comparison clips after matching RMS')
with wave.open(str(out/'RadioRoom-comparison.wav'),'wb') as w:
    w.setnchannels(2);w.setsampwidth(2);w.setframerate(sr);w.writeframes((comparison*32767).astype('<i2').tobytes())
(root/'tests/preview-report.json').write_text(json.dumps({'levels':metrics,'comparison_order':timeline,'note':'Original deterministic synthetic music fixture. RMS matched comparison; no subjective naturalness claim.'},indent=2))
print(json.dumps(metrics,indent=2))
