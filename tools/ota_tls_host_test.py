from pathlib import Path
import sys,datetime as dt,struct
sys.path.insert(0,str(Path(__file__).resolve().parent))
from check_official_ota import bundle_keys,verify_legacy_chain
from cryptography import x509
from cryptography.x509.oid import NameOID
from cryptography.hazmat.primitives import hashes,serialization
from cryptography.hazmat.primitives.asymmetric import ec
now=dt.datetime.now(dt.timezone.utc)
def issue(name,key,issuer=None,issuerkey=None,ca=True,expired=False):
 subject=x509.Name([x509.NameAttribute(NameOID.COMMON_NAME,name)])
 issuer=issuer or subject
 b=x509.CertificateBuilder().subject_name(subject).issuer_name(issuer).public_key(key.public_key()).serial_number(x509.random_serial_number()).not_valid_before(now-dt.timedelta(days=60)).not_valid_after(now+dt.timedelta(days=-1 if expired else 90)).add_extension(x509.BasicConstraints(ca=ca,path_length=None),critical=True)
 if not ca:b=b.add_extension(x509.SubjectAlternativeName([x509.DNSName('kiikoread.com')]),critical=False)
 return b.sign(issuerkey or key,hashes.SHA256())
def pem(*cs):return b''.join(c.public_bytes(serialization.Encoding.PEM) for c in cs)
def keys(*cs):return {c.subject.public_bytes():c.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo) for c in cs}
def bad(p,ks,match):
 try:verify_legacy_chain(p,ks,now)
 except Exception as e:
  assert match in str(e),(match,str(e));return
 raise AssertionError('Unexpected compatible chain')
rk=ec.generate_private_key(ec.SECP256R1());root=issue('modern root',rk)
ik=ec.generate_private_key(ec.SECP256R1());inter=issue('intermediate',ik,root.subject,rk)
lk=ec.generate_private_key(ec.SECP256R1());leaf=issue('kiikoread.com',lk,inter.subject,ik,False)
assert len(verify_legacy_chain(pem(leaf,inter),keys(root),now))==2
oldkey=ec.generate_private_key(ec.SECP256R1());old=issue('removed legacy root',oldkey);cross=issue('modern root',rk,old.subject,oldkey)
bad(pem(leaf,inter,cross),keys(root),'lacks terminal issuer')
assert len(verify_legacy_chain(pem(leaf,inter,cross),keys(root,old),now))==3
bad(pem(leaf,inter),keys(old),'lacks terminal issuer')
bad(pem(leaf,inter,issue('modern root',rk,expired=True)),keys(root),'validity')
bad(pem(leaf,root),keys(root),'not ordered')
dn=root.subject.public_bytes();key=keys(root)[dn];data=struct.pack('<IHH',4,len(dn),len(key))+dn+key;assert bundle_keys(data)==keys(root)
for malformed in (b'\x00'*8,struct.pack('<I',8)+b'\x00'*8):
 try:bundle_keys(malformed)
 except (AssertionError,ValueError,struct.error):pass
 else:raise AssertionError('accepted corrupt bundle')
print('PASS: compatible chain, rejected cross-signed legacy path, root key signatures, expired, unordered and malformed bundle; generated bundle bounds')
