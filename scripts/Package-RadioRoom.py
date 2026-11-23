from pathlib import Path
import argparse,hashlib,json,re,zipfile
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser();parser.add_argument('--tag');args=parser.parse_args()
version=re.search(r'project\(RadioRoom VERSION (\d+\.\d+\.\d+)',(root/'CMakeLists.txt').read_text()).group(1)
if args.tag and args.tag!='v'+version:raise SystemExit('Tag must match CMake project version')
out=root/'dist';out.mkdir(exist_ok=True)
reports={name:json.loads((root/'tests'/name).read_text()) for name in ['dsp-report.json','host-report.json','updater-report.json']}
if not all(r['passed'] for r in reports.values()):raise SystemExit('Verification failed: refusing to package')
exe=root/'build/RadioRoomStandalone_artefacts/Release/RadioRoom.exe'
vst=root/'build/RadioRoom_artefacts/Release/VST3/RadioRoom.vst3'
source=out/f'RadioRoom-{version}-Source.zip'
source_files=[]
for folder in ['Source','resources','third_party','scripts','.github','docs']:
    source_files.extend(p for p in (root/folder).rglob('*') if p.is_file() and '.git' not in p.parts and '__pycache__' not in p.parts)
source_files.extend(root/name for name in ['CMakeLists.txt','Build-source.ps1','.gitignore','README.md','LICENSE.txt','JUCE-LICENSE.md'])
with zipfile.ZipFile(source,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for p in source_files:z.write(p,'RadioRoom/'+p.relative_to(root).as_posix())
payload={}
for name in ['RadioRoom.exe','RadioRoom-Update.ps1','Release-Common.ps1','Install-RadioRoom.ps1','Uninstall-RadioRoom.ps1']:
    payload['Standalone/'+name]=exe if name=='RadioRoom.exe' else root/'scripts'/name
for name in ['README.md','LICENSE.txt','JUCE-LICENSE.md']:payload['Standalone/'+name]=root/name
for p in vst.rglob('*'):
    if p.is_file():payload['VST3/'+p.relative_to(vst.parent).as_posix()]=p
release={'schema':1,'product':'audio.lakam.radioroom','repository':'lakam99/RadioRoom','version':version,'platform':'windows-x64','files':{name:hashlib.sha256(p.read_bytes()).hexdigest() for name,p in payload.items()}}
(out/'release-manifest.json').write_text(json.dumps(release,indent=2))
package=out/f'RadioRoom-{version}-Windows-x64.zip'
with zipfile.ZipFile(package,'w',zipfile.ZIP_DEFLATED,compresslevel=6) as z:
    for name,p in payload.items():z.write(p,'RadioRoom/'+name)
    z.write(out/'release-manifest.json','RadioRoom/release-manifest.json')
    z.write(root/'scripts/Install-RadioRoom.ps1','RadioRoom/Install-RadioRoom.ps1')
    z.write(root/'scripts/Release-Common.ps1','RadioRoom/Release-Common.ps1')
    z.writestr('RadioRoom/Install-RadioRoom.cmd','@echo off\r\npowershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-RadioRoom.ps1" -PackageRoot "%~dp0."\r\npause\r\n')
    z.write(source,'RadioRoom/'+source.name)
    for name in reports:z.write(root/'tests'/name,'RadioRoom/verification/'+name)
    z.write(root/'tests/output/RadioRoom-skin-0.png','RadioRoom/RadioRoom-preview.png')
manifest={'version':version,'platform':'Windows x64','files':[]}
for p in [package,source,exe,vst/'Contents/x86_64-win/RadioRoom.vst3']:
    manifest['files'].append({'file':p.name,'path':str(p.relative_to(root)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(out/'manifest.json').write_text(json.dumps(manifest,indent=2))
(out/'SHA256SUMS.txt').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.name+'\n' for p in [package,source]))
print(json.dumps(manifest,indent=2))
