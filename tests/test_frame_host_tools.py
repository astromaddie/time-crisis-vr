#!/usr/bin/env python3
"""Check Frame build-host selection, including rejection of ARM64 Linux NDK use."""
from pathlib import Path
import sys
import tempfile
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from host_paths import frame_ndk_tool

with tempfile.TemporaryDirectory(prefix='tcvr-host-tools-') as folder:
    ndk = Path(folder)
    for host in ('darwin-x86_64', 'linux-x86_64'):
        tools = ndk/'toolchains/llvm/prebuilt'/host/'bin'
        tools.mkdir(parents=True)
        for name in ('clang', 'llvm-strip'):
            (tools/name).write_text('fixture for host path selection\n')
    for system, machine, expected in (
        ('darwin', 'arm64', 'darwin-x86_64'),
        ('darwin', 'x86_64', 'darwin-x86_64'),
        ('linux', 'x86_64', 'linux-x86_64'),
    ):
        with patch('host_paths.sys.platform', system), patch('host_paths.platform.machine', return_value=machine):
            for name in ('clang', 'llvm-strip'):
                assert frame_ndk_tool(ndk, name) == ndk/'toolchains/llvm/prebuilt'/expected/'bin'/name
            try:
                frame_ndk_tool(ndk, 'missing-tool')
            except RuntimeError as error:
                assert 'host tool not found' in str(error)
            else:
                raise AssertionError('Missing NDK tool was accepted')
    for system, machine in (('linux', 'aarch64'), ('win32', 'AMD64')):
        with patch('host_paths.sys.platform', system), patch('host_paths.platform.machine', return_value=machine):
            try:
                frame_ndk_tool(ndk, 'clang')
            except RuntimeError as error:
                assert 'WSL2' in str(error)
            else:
                raise AssertionError('Unsupported Frame build host was accepted')

print('PASS: macOS and Linux NDK selection, missing tools, and unsupported build hosts.')
