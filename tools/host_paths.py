"""Resolve standard host tools without machine-specific user paths."""
from pathlib import Path
import os
import platform
import shutil
import subprocess
import sys


def android_sdk_root():
    configured = os.environ.get('ANDROID_SDK_ROOT') or os.environ.get('ANDROID_HOME')
    if configured:
        return Path(configured).expanduser()
    candidates = ((Path.home()/'Library/Android/sdk', Path.home()/'Android/Sdk')
                  if sys.platform == 'darwin' else
                  (Path.home()/'Android/Sdk', Path.home()/'Library/Android/sdk'))
    return next((path for path in candidates if path.is_dir()), candidates[0])


def java_home():
    configured = os.environ.get('JAVA_HOME')
    if configured:
        return Path(configured).expanduser()
    if sys.platform == 'darwin':
        brew = shutil.which('brew')
        if brew:
            for formula in ('openjdk@17', 'openjdk'):
                result = subprocess.run([brew, '--prefix', formula], capture_output=True, text=True)
                if result.returncode == 0 and result.stdout.strip():
                    candidate = Path(result.stdout.strip())
                    if (candidate/'bin/java').is_file() and (candidate/'bin/javac').is_file():
                        return candidate
    javac = shutil.which('javac')
    if javac:
        candidate = Path(javac).resolve().parent.parent
        if (candidate/'bin/java').is_file():
            return candidate
    java = shutil.which('java')
    if java:
        candidate = Path(java).resolve().parent.parent
        if (candidate/'bin/javac').is_file():
            return candidate
    raise RuntimeError('JDK 17 is required; set JAVA_HOME or put java and javac on PATH')


def java_tool(name):
    path = java_home()/'bin'/name
    if not path.is_file():
        raise RuntimeError(f'JDK tool not found: {path}')
    return path


def frame_ndk_tool(ndk, name):
    """Select tools for the build computer, independently of the ARM64 APK target."""
    if sys.platform == 'darwin':
        host = 'darwin-x86_64'
    elif sys.platform == 'linux' and platform.machine().lower() in ('x86_64', 'amd64'):
        host = 'linux-x86_64'
    else:
        raise RuntimeError('Build the Frame APK on macOS or x86_64 Linux; on Windows use x86_64 WSL2. The official Linux NDK does not run natively on the ARM64 Frame.')
    tool = Path(ndk)/'toolchains/llvm/prebuilt'/host/'bin'/name
    if not tool.is_file():
        raise RuntimeError(f'Android NDK host tool not found: {tool}')
    return tool


def executable(name, environment=None):
    configured = os.environ.get(environment or name.upper())
    candidate = configured or shutil.which(name)
    if not candidate:
        raise RuntimeError(f'{name} is required; install it or set {environment or name.upper()}')
    path = Path(candidate).expanduser()
    if not path.is_file():
        resolved = shutil.which(candidate)
        if resolved:
            path = Path(resolved)
    if not path.is_file():
        raise RuntimeError(f'{name} executable not found: {path}')
    return path
