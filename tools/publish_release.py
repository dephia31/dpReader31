#!/usr/bin/env python3
"""中文：先校验并上传完整草稿，再公开正式Release；不覆盖已发布字节。/ English: Validate/upload a complete draft before publication; never overwrite published bytes."""
from pathlib import Path
import argparse,hashlib,json,os,subprocess,urllib.request,urllib.error
from verify_metalio_release import verify
ROOT=Path(__file__).resolve().parents[1]
REPO='dephia31/dpReader31'
def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--dry-run',action='store_true');args=parser.parse_args()
    manifest=verify();version=manifest['version'];tag='v'+version
    names=['dpReader_metalio47_firmware.bin','dpReader_metalio47_full.bin','SHA256SUMS.txt','metalio-s31-update.json','verification.json',f'dpReader-metalio47-{version}-USB.zip',f'dpReader-metalio47-{version}-TF.zip']
    assert all((ROOT/'releases'/n).is_file() for n in names)
    if args.dry_run:print('Ready to publish',tag,':',', '.join(names));return
    if os.environ.get('GITHUB_REF_TYPE')=='tag' and os.environ.get('GITHUB_REF_NAME')!=tag:
        raise RuntimeError('Workflow tag differs from release manifest')
    head=subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip()
    tagged=subprocess.check_output(['git','rev-parse',tag+'^{commit}'],cwd=ROOT,text=True).strip()
    if head!=tagged:raise RuntimeError('Release tag does not match checked-out source')
    token=os.environ.get('GITHUB_TOKEN')
    if not token:
        r=subprocess.run(['git','credential','fill'],input='protocol=https\nhost=github.com\n\n',text=True,capture_output=True,check=True,env={**os.environ,'GIT_TERMINAL_PROMPT':'0'})
        token=dict(line.split('=',1) for line in r.stdout.splitlines() if '=' in line).get('password')
    if not token:raise RuntimeError('GitHub credential required')
    def api(route,method='GET',body=None,raw=None,kind='application/json'):
        headers={'Authorization':'Bearer '+token,'Accept':'application/vnd.github+json','User-Agent':'dpReader-release','Content-Type':kind}
        if body is not None:raw=json.dumps(body).encode()
        url=route if route.startswith('https://') else 'https://api.github.com/repos/'+REPO+'/'+route
        with urllib.request.urlopen(urllib.request.Request(url,data=raw,headers=headers,method=method),timeout=90) as response:return json.load(response)
    notes=(ROOT/'releases/RELEASE_NOTES.md').read_text()
    try:release=api('releases/tags/'+tag)
    except urllib.error.HTTPError as error:
        if error.code!=404:raise
        release=api('releases','POST',{'tag_name':tag,'name':'dpReader Metalio 4.7 Firmware '+tag,'body':notes,'draft':True,'prerelease':False})
    for name in names:
        data=(ROOT/'releases'/name).read_bytes();digest='sha256:'+hashlib.sha256(data).hexdigest()
        old=next((a for a in release['assets'] if a['name']==name),None)
        if old:
            if old.get('digest')!=digest or old['size']!=len(data):raise RuntimeError('Immutable asset differs: '+name)
            continue
        if not release['draft']:raise RuntimeError('Published release is incomplete; review before adding assets')
        asset=api(release['upload_url'].split('{')[0]+'?name='+name,'POST',raw=data,kind='application/octet-stream')
        if asset.get('digest')!=digest or asset['size']!=len(data):raise RuntimeError('Upload digest mismatch: '+name)
        print('Verified upload:',name)
    release=api('releases/'+str(release['id']),'PATCH',{'draft':False,'prerelease':False,'make_latest':'true','body':notes})
    print('Published:',release['html_url'])
if __name__=='__main__':main()
