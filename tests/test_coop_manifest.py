import importlib.util
import xml.etree.ElementTree as ET
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
spec = importlib.util.spec_from_file_location('time_crisis_build', ROOT / 'tools/build.py')
build = importlib.util.module_from_spec(spec)
spec.loader.exec_module(build)

ANDROID = '{http://schemas.android.com/apk/res/android}'
ACTIVITY = '{http://schemas.android.com/apk/res/android}name'

for bundled in (False, True):
    manifest = build.package_manifest(bundled)
    tree = ET.parse(manifest)
    app = tree.getroot().find('application')
    activities = {entry.get(ACTIVITY): entry for entry in app.findall('activity')}
    setup = activities['.LauncherActivity']
    main = activities['.MainActivity']

    def has_filter(activity, action, category):
        return any(
            any(item.get(ACTIVITY) == action for item in intent.findall('action'))
            and any(item.get(ACTIVITY) == category for item in intent.findall('category'))
            for intent in activity.findall('intent-filter')
        )

    assert has_filter(setup, 'android.intent.action.MAIN', 'android.intent.category.LAUNCHER')
    assert not has_filter(main, 'android.intent.action.MAIN', 'android.intent.category.LAUNCHER')
    assert has_filter(main, 'android.intent.action.MAIN', 'org.khronos.openxr.intent.category.IMMERSIVE_HMD')
    assert has_filter(main, 'android.intent.action.MAIN', 'com.oculus.intent.category.VR')

frame = ET.parse(ROOT / 'quest/AndroidManifest-SteamFrame.xml').getroot()
frame_activities = {entry.get(ACTIVITY): entry for entry in frame.find('application').findall('activity')}
assert has_filter(frame_activities['.LauncherActivity'], 'android.intent.action.MAIN', 'android.intent.category.LAUNCHER')
assert has_filter(frame_activities['.MainActivity'], 'android.intent.action.MAIN', 'org.khronos.openxr.intent.category.IMMERSIVE_HMD')
print('PASS: ordinary Quest launch enters verified setup for both package variants; MainActivity retains Quest and Steam Frame VR entry filters.')
