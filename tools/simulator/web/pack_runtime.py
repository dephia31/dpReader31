"""Fingerprint engine assets and precompress the preload data for Safari."""
from pathlib import Path
import gzip,hashlib,json,sys,shutil
root=Path(__file__).resolve().parent
def pack(dist):
    asset={}
    for kind,source in [('module','pico.js'),('wasm','pico.wasm'),('data','pico.data')]:
        raw=(dist/source).read_bytes();tag=hashlib.sha256(raw).hexdigest()[:16]
        name='pico-'+tag+('.js' if kind=='module' else '.wasm' if kind=='wasm' else '.data.gz')
        (dist/name).write_bytes(gzip.compress(raw,compresslevel=9,mtime=0) if kind=='data' else raw);asset[kind]=name;asset[kind+'Size']=len(raw)
    (dist/'runtime.json').write_text(json.dumps(asset,indent=2)+'\n')
    shutil.copy2(root/'boot.js',dist/'boot.js')
    print('Data transfer bytes:',(dist/asset['data']).stat().st_size,'from',(dist/'pico.data').stat().st_size)
if __name__=='__main__':pack(Path(sys.argv[1]) if len(sys.argv)>1 else root/'dist')
