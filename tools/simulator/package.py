from pathlib import Path
import shutil,plistlib,subprocess
r=Path(__file__).resolve().parent; app=r/'Pico Simulator.app';contents=app/'Contents';mac=contents/'MacOS';resources=contents/'Resources'
mac.mkdir(parents=True,exist_ok=True);resources.mkdir(exist_ok=True)
subprocess.run(['swiftc','-module-cache-path',str(r/'build/swift-cache'),'-O','-framework','Cocoa',str(r/'src/PicoSimulator.swift'),'-o',str(mac/'PicoSimulator')],check=True)
shutil.copy2(r/'build/pico-engine',resources/'pico-engine');shutil.copy2(r/'source-manifest.json',resources/'source-manifest.json')
with (contents/'Info.plist').open('wb') as f:plistlib.dump({'CFBundleName':'Pico Simulator','CFBundleDisplayName':'Pico 模拟器','CFBundleIdentifier':'local.pico.simulator','CFBundleVersion':'2','CFBundleShortVersionString':'0.2','CFBundleExecutable':'PicoSimulator','CFBundlePackageType':'APPL','NSHighResolutionCapable':True,'LSMinimumSystemVersion':'13.0'},f)
subprocess.run(['codesign','--force','--sign','-',str(app)],check=True)
print(app)
