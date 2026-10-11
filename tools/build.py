"""Quest build: ROM-free APK by default, optional APK bundling supplied ROMs."""
from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, tempfile, zipfile
import xml.etree.ElementTree as ET
import bootstrap, patch_sdl_frame, patch_upstream, translate_crate
ROOT=Path(__file__).resolve().parents[1]
FRAME_BUILD=os.environ.get('TCVR_STEAM_FRAME_BUILD')=='1'
UPSTREAM=Path(os.environ.get('TCVR_UPSTREAM_SOURCE',ROOT/'upstream'))
BUILD=ROOT/('build/frame' if FRAME_BUILD else 'build')
JAVA=Path(os.environ.get('JAVA_HOME',r'C:\Program Files\Android\Android Studio\jbr'))/'bin'
def run(args,cwd=ROOT):
    print('> '+' '.join(map(str,args)),flush=True)
    subprocess.run(list(map(str,args)),cwd=cwd,check=True)

def tree_sha256(root):
    """Hash source files in the disposable upstream view, excluding ROM-derived game data."""
    digest=hashlib.sha256()
    for path in sorted(root.rglob('*')):
        if not path.is_file():continue
        rel=path.relative_to(root)
        if '.git' in rel.parts or 'extracted' in rel.parts or (rel.parts[:2]==('timecris','gen')):continue
        digest.update(rel.as_posix().encode('utf-8')+b'\0')
        digest.update(path.read_bytes())
    return digest.hexdigest()

def source_worktree_sha256():
    digest=hashlib.sha256()
    diff=subprocess.check_output(['git','diff','--binary','HEAD'],cwd=ROOT)
    digest.update(diff)
    untracked=subprocess.check_output(['git','ls-files','--others','--exclude-standard','-z'],cwd=ROOT).decode().split('\0')
    for name in sorted(item for item in untracked if item):
        path=ROOT/name
        if path.is_file():digest.update(name.encode()+b'\0'+path.read_bytes())
    return digest.hexdigest()

def command_version(command, *args):
    """Capture a compact, exact tool identity for the Frame build receipt."""
    result=subprocess.run([str(command),*args],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,check=True)
    return result.stdout.strip().splitlines()[0]

def sdk_properties(path):
    values={}
    if path.is_file():
        for line in path.read_text(errors='replace').splitlines():
            line=line.strip()
            if line and not line.startswith('#') and '=' in line:
                key,value=line.split('=',1);values[key.strip()]=value.strip()
    return values

def prepare(rom):
    patch_upstream.main()
    tc=UPSTREAM/'timecris'
    run([sys.executable,tc/'tools/setup_roms.py',rom])
    run([sys.executable,'../tools/gen/c25_translate.py','--game','tc','--roms','extracted','--main-rom','timecris_main.bin','--cov','tools/gen/c25.cov','--out','gen/tc_c25.c','--func','tc_c25_exec'],tc)
    assets=BUILD/'assets'; roms=assets/'roms';roms.mkdir(parents=True,exist_ok=True)
    lines=[]
    for src in sorted((tc/'extracted').iterdir()):
        if not src.is_file():continue
        shutil.copy2(src,roms/src.name)
        lines.append(hashlib.sha256(src.read_bytes()).hexdigest()+'  '+src.name)
    (assets/'roms.sha256').write_text('\n'.join(lines)+'\n',encoding='ascii')
    return assets

def prepare_sound():
    """Keep runtime-discovered sound entry points in a reproducible ROM translation."""
    tc=UPSTREAM/'timecris'
    coverage=BUILD/'quest-snd.cov'
    coverage.write_text((tc/'tools/gen/snd.cov').read_text().rstrip()+'\n'+(ROOT/'quest/snd_extra.cov').read_text())
    generated=BUILD/'tc_snd_driver.c'
    run([sys.executable,UPSTREAM/'tools/gen/snd_translate.py',coverage,'--game','tc','--roms',tc/'extracted','--out',generated],tc)
    target=tc/'gen/tc_snd_driver.c'
    if not target.exists() or target.read_bytes()!=generated.read_bytes():shutil.copy2(generated,target)
    translate_crate.main()

def package_manifest(bundle_roms):
    """The OpenXR activity must retain its own VR categories in both variants."""
    if FRAME_BUILD:return ROOT/'quest/AndroidManifest-SteamFrame.xml'
    ET.register_namespace('android','http://schemas.android.com/apk/res/android')
    tree=ET.parse(ROOT/'quest/AndroidManifest.xml');ns='{http://schemas.android.com/apk/res/android}'
    activities={a.get(ns+'name'):a for a in tree.getroot().find('application').findall('activity')}
    main=activities['.MainActivity'];setup=activities['.LauncherActivity']
    categories={c.get(ns+'name') for c in main.findall('intent-filter/category')}
    assert {'android.intent.category.LAUNCHER','org.khronos.openxr.intent.category.IMMERSIVE_HMD','com.oculus.intent.category.VR'}<=categories
    if not bundle_roms:
        # Setup is a separate 2D task. Its handoff targets the VR-marked activity.
        for intent in main.findall('intent-filter'):
            for category in list(intent.findall('category')):
                if category.get(ns+'name')=='android.intent.category.LAUNCHER':intent.remove(category)
        intent=ET.SubElement(setup,'intent-filter')
        ET.SubElement(intent,'action',{ns+'name':'android.intent.action.MAIN'})
        ET.SubElement(intent,'category',{ns+'name':'android.intent.category.LAUNCHER'})
    output=BUILD/('AndroidManifest-bundled.xml' if bundle_roms else 'AndroidManifest-rom-free.xml')
    tree.write(output,encoding='utf-8',xml_declaration=True)
    return output

def sdl_java_source_root(sdl_source, generated_view=None):
    """Keep SDL source pristine for Quest; isolate Frame's Lepton Java guard."""
    java_root=Path(sdl_source)/'android-project/app/src/main/java'
    if not FRAME_BUILD:return java_root,None
    generated_view=generated_view or BUILD/'generated-sdl-java'
    metadata=patch_sdl_frame.prepare_frame_java_view(java_root,generated_view)
    return Path(generated_view),metadata

def package(assets):
    models=assets/'models';models.mkdir(exist_ok=True)
    model=ROOT/'quest/assets/models/player-gun.tcgun'
    if not model.is_file():raise RuntimeError('Missing player gun asset: '+str(model))
    shutil.copy2(model,models/model.name)
    (assets/'models.sha256').write_text(hashlib.sha256(model.read_bytes()).hexdigest()+'  '+model.name+'\n',encoding='ascii')
    if FRAME_BUILD:
        sdk=Path(os.environ['ANDROID_SDK_ROOT']);bt=sdk/'build-tools/35.0.0';android=sdk/'platforms/android-34/android.jar'
        d8=bt/'lib/d8.jar';strip=Path(os.environ['ANDROID_NDK_ROOT'])/'toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-strip'
        sdl_source=ROOT/'.tools/sdl/SDL2-2.30.11';openxr=ROOT/'.tools/openxr';
    else:
        bt=ROOT/'.tools/buildtools/android-15';android=ROOT/'.tools/platform/android-34-ext12/android.jar'
        d8=bt/'lib/d8.jar';strip=ROOT/'.tools/ndk/android-ndk-r27c/toolchains/llvm/prebuilt/windows-x86_64/bin/llvm-strip.exe'
        sdl_source=ROOT/'.tools/sdl/SDL2-2.30.11';openxr=ROOT/'.tools/openxr'
    classes=BUILD/'classes';classes.mkdir(exist_ok=True)
    sdl_java_root,frame_sdl_metadata=sdl_java_source_root(sdl_source)
    sources=list(sdl_java_root.rglob('*.java'))+list((ROOT/'quest/java').rglob('*.java'))
    argfile=BUILD/'javac.args'
    argfile.write_text('\n'.join('"'+str(p).replace('\\','/')+'"' for p in sources))
    javac=JAVA/('javac' if FRAME_BUILD else 'javac.exe');jar_tool=JAVA/('jar' if FRAME_BUILD else 'jar.exe');java=JAVA/('java' if FRAME_BUILD else 'java.exe')
    run([javac,'-source','8','-target','8','-classpath',android,'-d',classes,'@'+str(argfile)])
    jar=BUILD/'classes.jar';run([jar_tool,'--create','--file',jar,'-C',classes,'.'])
    dex=BUILD/'dex';dex.mkdir(exist_ok=True)
    run([java,'-cp',d8,'com.android.tools.r8.D8','--lib',android,'--min-api','29','--output',dex,jar])
    licenses=assets/'licenses';licenses.mkdir(exist_ok=True)
    for source,name in [(ROOT/'LICENSE','TimeCrisisVR-LICENSE.txt'),(ROOT/'NOTICE.md','NOTICE.md'),(UPSTREAM/'LICENSE','namco22-LICENSE.txt'),(sdl_source/'LICENSE.txt','SDL2-LICENSE.txt'),(openxr/'META-INF/LICENSE','OpenXR-LICENSE.txt')]:
        shutil.copy2(source,licenses/name)
    base=BUILD/'unaligned.apk'
    # Whitelist public assets into a fresh directory; stale build ROMs cannot leak.
    with tempfile.TemporaryDirectory(prefix='public-assets-',dir=BUILD) as temp:
        public=Path(temp)
        for name in ('models','licenses'):shutil.copytree(assets/name,public/name)
        shutil.copy2(assets/'models.sha256',public/'models.sha256')
        if args.bundle_roms and not FRAME_BUILD:shutil.copytree(assets/'roms',public/'roms')
        # c71.bin is optional and already supplied by the upstream engine.
        manifest=[line for line in (assets/'roms.sha256').read_text().splitlines() if line.split('  ',1)[1]!='c71.bin']
        assert len(manifest)==31
        (public/'roms.sha256').write_text('\n'.join(manifest)+'\n',encoding='ascii')
        run([bt/('aapt2' if FRAME_BUILD else 'aapt2.exe'),'link','--manifest',package_manifest(args.bundle_roms),'-I',android,'-A',public,'-o',base])
    libs=BUILD/'package-libs';libs.mkdir(exist_ok=True)
    for name,source in [('libmain.so',BUILD/'native/libmain.so'),('libSDL2.so',BUILD/'native/sdl/libSDL2.so'),('libopenxr_loader.so',openxr/'prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so')]:
        run([strip,'--strip-unneeded','-o',libs/name,source])
    with zipfile.ZipFile(base,'a',zipfile.ZIP_DEFLATED) as z:
        if not args.bundle_roms:assert not any(n.startswith('assets/roms/') or n.endswith('timecris.zip') for n in z.namelist())
        for p in dex.glob('*.dex'):z.write(p,p.name)
        for p in libs.glob('*.so'):z.write(p,'lib/arm64-v8a/'+p.name)
    output=ROOT/('artifacts/TimeCrisisVR-SteamFrame-arm64.apk' if FRAME_BUILD else ('artifacts/bundled/TimeCrisisVR-with-ROM.apk' if args.bundle_roms else 'artifacts/TimeCrisisVR-quest3-debug.apk'));output.parent.mkdir(parents=True,exist_ok=True)
    zipalign=bt/('zipalign' if FRAME_BUILD else 'zipalign.exe');apksigner=bt/('apksigner' if FRAME_BUILD else 'apksigner.exe')
    run([zipalign,'-f','-P','16','4',base,output])
    if FRAME_BUILD:
        key=Path.home()/'.config/time-crisis-vr/frame-dev.keystore';key.parent.mkdir(parents=True,exist_ok=True)
        if not key.exists():
            run([JAVA/'keytool','-genkeypair','-keystore',key,'-storepass','android','-alias','frame-dev','-keypass','android','-keyalg','RSA','-keysize','2048','-validity','10000','-dname','CN=TimeCrisisVR Dev,O=TimeCrisisVR,C=US'])
            key.chmod(0o600)
        alias='frame-dev'
    else:
        key=ROOT/'.tools/debug.keystore'
        if not key.exists():run([JAVA/'keytool.exe','-genkeypair','-keystore',key,'-storepass','android','-alias','androiddebugkey','-keypass','android','-keyalg','RSA','-keysize','2048','-validity','10000','-dname','CN=Android Debug,O=Android,C=US'])
        alias='androiddebugkey'
    signer=[java,'-jar',bt/'lib/apksigner.jar']
    run(signer+['sign','--ks',key,'--ks-key-alias',alias,'--ks-pass','pass:android','--key-pass','pass:android',output])
    run(signer+['verify','--verbose',output]);run([zipalign,'-c','-P','16','4',output])
    report={'apk':output.name,'sha256':hashlib.sha256(output.read_bytes()).hexdigest(),'bytes':output.stat().st_size,'upstream':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT/'upstream',text=True).strip(),'abi':'arm64-v8a','roms_bundled':False if FRAME_BUILD else args.bundle_roms,'hardware_tested':False}
    if FRAME_BUILD:
        sdk=Path(os.environ['ANDROID_SDK_ROOT']);ndk=Path(os.environ['ANDROID_NDK_ROOT'])
        import bootstrap_frame
        sdl_archive=ROOT/'.tools/downloads/SDL2-2.30.11.tar.gz'
        xr_archive=ROOT/'.tools/downloads/openxr_loader_for_android-1.1.43.aar'
        loader=ROOT/'.tools/openxr/prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so'
        cmake=os.environ.get('CMAKE','cmake');ninja=os.environ.get('NINJA','ninja')
        clang=ndk/'toolchains/llvm/prebuilt/darwin-x86_64/bin/clang'
        report.update({
            'package':'org.timecrisis.frame',
            'source_revision':subprocess.check_output(['git','rev-parse','HEAD'],cwd=ROOT,text=True).strip(),
            'source_worktree_sha256':source_worktree_sha256(),
            'generated_upstream_tree':str(UPSTREAM.relative_to(ROOT)),
            'upstream_tree_sha256':tree_sha256(UPSTREAM),
            'patch_script_sha256':hashlib.sha256((ROOT/'tools/patch_upstream.py').read_bytes()).hexdigest(),
            'build_tools':{
                'python':{'path':sys.executable,'version':sys.version.split()[0]},
                'jdk':{'path':str(JAVA.parent),'java':command_version(JAVA/'java','-version'),'javac':command_version(JAVA/'javac','-version')},
                'cmake':{'path':str(cmake),'version':command_version(cmake,'--version')},
                'ninja':{'path':str(ninja),'version':command_version(ninja,'--version')},
                'android_sdk_platform':sdk_properties(sdk/'platforms/android-34/source.properties'),
                'android_build_tools':sdk_properties(sdk/'build-tools/35.0.0/source.properties'),
                'android_ndk':sdk_properties(ndk/'source.properties'),
                'android_clang':{'path':str(clang),'version':command_version(clang,'--version')},
            },
            'dependencies':{
                'sdl':{'version':'2.30.11','url':bootstrap_frame.SDL_URL,'sha256':bootstrap_frame.SDL_SHA256,
                       'archive_sha256':hashlib.sha256(sdl_archive.read_bytes()).hexdigest()},
                'openxr_android_loader':{'version':'1.1.43','url':bootstrap_frame.XR_URL,'sha256':bootstrap_frame.XR_SHA256,
                                         'archive_sha256':hashlib.sha256(xr_archive.read_bytes()).hexdigest(),
                                         'arm64_library_sha256':hashlib.sha256(loader.read_bytes()).hexdigest()},
            },
            'sdl_frame_java':{
                **frame_sdl_metadata,
                'generated_source_view':str(sdl_java_root.relative_to(ROOT)),
                'patch_script_sha256':hashlib.sha256((ROOT/'tools/patch_sdl_frame.py').read_bytes()).hexdigest(),
            },
            'native_build':'arm64-v8a, separate-eye default, absolute 2160-pixel eye cap',
            'signing':'persistent local development key; excluded from APK',
        })
    else:
        report['rom_sha256']=hashlib.sha256(args.rom.read_bytes()).hexdigest()
    manifest_path=ROOT/('quest/AndroidManifest-SteamFrame.xml' if FRAME_BUILD else 'quest/AndroidManifest.xml')
    manifest=ET.parse(manifest_path).getroot();ns='{http://schemas.android.com/apk/res/android}'
    report['version_name']=manifest.get(ns+'versionName');report['version_code']=int(manifest.get(ns+'versionCode'))
    (output.parent/('TimeCrisisVR-SteamFrame-build-info.json' if FRAME_BUILD else 'build-info.json')).write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps(report,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--rom',type=Path,required=True,help='user-supplied matching Time Crisis ROM ZIP');p.add_argument('--jobs',type=int,default=2);p.add_argument('--package-only',action='store_true');p.add_argument('--skip-prepare',action='store_true');p.add_argument('--bundle-roms',action='store_true',help='Include the supplied ROM files in the APK');args=p.parse_args()
    if not args.rom.is_file():p.error('ROM not found: '+str(args.rom))
    BUILD.mkdir(parents=True,exist_ok=True)
    if not args.package_only:
        if FRAME_BUILD:
            import bootstrap_frame
            bootstrap_frame.main()
        else:
            for item in bootstrap.PACKAGES:bootstrap.fetch(item)
        patch_upstream.main()
        assets=BUILD/'assets' if args.skip_prepare else prepare(args.rom)
        prepare_sound()
        if FRAME_BUILD:
            ndk=Path(os.environ['ANDROID_NDK_ROOT'])
            run([os.environ.get('CMAKE','cmake'),'-Wno-dev','-Wno-deprecated','-S',ROOT/'quest','-B',BUILD/'native','-G','Ninja',
                 '-DCMAKE_MAKE_PROGRAM='+os.environ.get('NINJA','ninja'),'-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),
                 '-DTCVR_UPSTREAM_SOURCE='+str(UPSTREAM),'-DTCVR_STEAM_FRAME=ON','-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29',
                 '-DANDROID_STL=c++_static','-DCMAKE_BUILD_TYPE=Release'])
        else:
            ndk=ROOT/'.tools/ndk/android-ndk-r27c'
            run(['cmake','-Wno-dev','-Wno-deprecated','-S',ROOT/'quest','-B',BUILD/'native','-G','Ninja','-DCMAKE_MAKE_PROGRAM='+str(ROOT/'.tools/ninja/ninja.exe'),'-DCMAKE_TOOLCHAIN_FILE='+str(ndk/'build/cmake/android.toolchain.cmake'),'-DANDROID_ABI=arm64-v8a','-DANDROID_PLATFORM=android-29','-DANDROID_STL=c++_static','-DCMAKE_BUILD_TYPE=Release'])
        run(['cmake','--build',BUILD/'native','--parallel',args.jobs])
    else:assets=BUILD/'assets'
    package(assets)
