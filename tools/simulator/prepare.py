"""Create an immutable rc97 source snapshot only when no snapshot exists."""
from pathlib import Path
import os, shutil, json, hashlib
root=Path(__file__).resolve().parent
base=Path(os.environ.get('PICO_RELEASE_SOURCE',root.parent/'read-pico-reader/build/rc97-release-20261018/source'))
fw=root/'firmware'
if fw.exists():raise SystemExit('Snapshot exists; verify source-manifest.json instead of replacing it.')
shutil.copytree(base/'main',fw/'main')
for directory in (base/'components').iterdir():
    if not directory.is_dir():continue
    for file in directory.rglob('*.h'):
        dst=fw/'components'/directory.name/file.relative_to(directory)
        dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(file,dst)
search=base/'components/read_pico_search/read_pico_search.c'
shutil.copy2(search,fw/'components/read_pico_search/read_pico_search.c')
for name in ['LICENSE','NOTICE']:
    if (base/name).exists():shutil.copy2(base/name,fw/name)
files={str(p.relative_to(fw)):hashlib.sha256(p.read_bytes()).hexdigest() for p in fw.rglob('*') if p.is_file()}
(root/'source-manifest.json').write_text(json.dumps({'version':'0.3.3-rc97','release_commit':'0ae3ef80a3f391bb892248f324cf0835318ddbba','snapshot_files':files},ensure_ascii=False,indent=2))
print('Snapshot:',len(files),'files')
