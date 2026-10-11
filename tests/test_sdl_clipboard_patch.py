"""Exercise Frame-only SDL patching against the pinned source and a host Android clipboard stub."""
from pathlib import Path
import hashlib
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
from host_paths import android_sdk_root, java_tool
import patch_sdl_frame
import build as build_tools

SDL_ROOT = ROOT/'.tools/sdl/SDL2-2.30.11'
SDL_SOURCE = SDL_ROOT/'android-project/app/src/main/java'
HANDLER_SOURCE = SDL_SOURCE/patch_sdl_frame.HANDLER_PATH
JAVAC = java_tool('javac')
JAVA = java_tool('java')
SDK = android_sdk_root()
ANDROID_JAR = SDK/'platforms/android-34/android.jar'

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

if not HANDLER_SOURCE.is_file():
    raise SystemExit(f'Pinned SDL source not found: {HANDLER_SOURCE}')
if not ANDROID_JAR.is_file():
    raise SystemExit(f'Android API 34 jar not found: {ANDROID_JAR}')
source_hash = digest(HANDLER_SOURCE)

with tempfile.TemporaryDirectory(prefix='tcvr-sdl-clipboard-') as temp:
    temp = Path(temp)
    # The ordinary Quest path must continue to compile directly from pristine SDL.
    build_tools.FRAME_BUILD = False
    unmodified_root, unmodified_metadata = build_tools.sdl_java_source_root(SDL_ROOT, temp/'quest-view')
    assert unmodified_root == SDL_SOURCE and unmodified_metadata is None
    assert patch_sdl_frame.PATCH_MARKER not in HANDLER_SOURCE.read_text()

    # The Frame path uses an isolated copy, and the transformation is stable and idempotent.
    build_tools.FRAME_BUILD = True
    generated_root, metadata = build_tools.sdl_java_source_root(SDL_ROOT, temp/'frame-view')
    patched_handler = generated_root/patch_sdl_frame.HANDLER_PATH
    patched_bytes = patched_handler.read_bytes()
    second_patch = patch_sdl_frame.patch_handler_file(patched_handler)
    assert patched_handler.read_bytes() == patched_bytes
    assert metadata['patch_id'] == patch_sdl_frame.PATCH_ID
    assert metadata['source_sha256'] == source_hash
    assert metadata['patched_sha256'] == digest(patched_handler)
    assert metadata['generated_java_tree_sha256'] == patch_sdl_frame.java_tree_sha256(generated_root)
    assert second_patch['patched_sha256'] == metadata['patched_sha256']
    assert digest(HANDLER_SOURCE) == source_hash

    # Compile the actual patched class with small stand-ins for Android's clipboard API.
    source_dir = temp/'src'
    classes_dir = temp/'classes'
    (source_dir/'org/libsdl/app').mkdir(parents=True)
    classes_dir.mkdir()
    for stub in (ROOT/'tests/sdl_clipboard_stubs').rglob('*.java'):
        destination = source_dir/stub.relative_to(ROOT/'tests/sdl_clipboard_stubs')
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(stub.read_bytes())
    source_text = patched_handler.read_text()
    handler = source_text[source_text.index(patch_sdl_frame.PATCH_MARKER):]
    extracted = source_dir/'org/libsdl/app/SDLClipboardHandler.java'
    extracted.write_text('''package org.libsdl.app;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
''' + handler)
    java_sources = sorted(source_dir.rglob('*.java'))+[extracted]
    subprocess.run([str(JAVAC), '-source', '8', '-target', '8', '-d', str(classes_dir),
                    *map(str, java_sources)], check=True)
    subprocess.run([str(JAVA), '-cp', str(classes_dir), 'org.libsdl.app.ClipboardProbe'], check=True)

    # Compile the original source list like the non-Frame package path; it must not see the patch.
    quest_classes = temp/'quest-classes'
    quest_classes.mkdir()
    quest_sources = sorted(SDL_SOURCE.rglob('*.java'))+sorted((ROOT/'quest/java').rglob('*.java'))
    subprocess.run([str(JAVAC), '-source', '8', '-target', '8', '-classpath', str(ANDROID_JAR),
                    '-d', str(quest_classes), *map(str, quest_sources)], check=True)
    assert digest(HANDLER_SOURCE) == source_hash

print('PASS: Frame copy is patched and hashed; Quest source remains pristine; null and present clipboard paths compile and behave correctly')
