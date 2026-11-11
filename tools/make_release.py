"""Makes the download for a GitHub release, which RecompLauncher installs: out/release/Anarchy-Reigns-Supreme-<version>-Windows.zip.

Usage: python tools/make_release.py v0.1.0

It holds the committed files (so uncommitted changes are left out) with recomplauncher.json at the top, and ARSetup.exe,
built here from tools/setup, so a player can start Setup without building it first. Nothing from the game is in it:
the game is built on the player's PC from their own copy. Checks for that are run before the zip is written.
"""
import hashlib
import os
import re
import subprocess
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'out', 'release')
# Made from the game or by a build, and never to be shared.
FORBIDDEN = re.compile(r'(^|/)(ARRecompLib/ppc/|ARRecompLib/private/.+(?<!\.gitkeep)$|out/|run/|user/)|\.(xex|iso|xcp|stfs|pdb|dll)$', re.I)


def build_setup():
    """ARSetup.exe with the Visual Studio C++ tools, as Setup.bat builds it."""
    vswhere = os.path.expandvars(r'%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe')
    vs = ''
    for version in (['-version', '[17.0,18.0)'], []):
        vs = subprocess.run([vswhere, '-products', '*', *version, '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-latest',
                             '-property', 'installationPath'], capture_output=True, text=True).stdout.strip()
        if vs:
            break
    if not vs:
        sys.exit('The Visual Studio C++ tools are needed to build ARSetup.exe.')
    script = os.path.join(OUT, 'build_setup.bat')
    with open(script, 'w') as f:
        f.write('@echo off\r\n')
        f.write(f'call "{vs}\\VC\\Auxiliary\\Build\\vcvars64.bat" >nul || exit /b 1\r\n')
        f.write(f'cd /d "{ROOT}"\r\n')
        f.write('cl /nologo /std:c++20 /EHsc /O2 /MT /utf-8 /DUNICODE /D_UNICODE /Itools\\ui /Fo:out\\release\\ /Fe:out\\release\\ARSetup.exe '
                'tools\\setup\\main.cpp tools\\setup\\package.cpp tools\\ui\\theme.cpp /link /subsystem:windows user32.lib gdi32.lib comctl32.lib '
                'comdlg32.lib ole32.lib shell32.lib bcrypt.lib\r\n')
    result = subprocess.run(['cmd', '/c', script], capture_output=True, text=True)
    if result.returncode != 0:
        sys.exit('ARSetup.exe did not build:\n' + result.stdout + result.stderr)
    return os.path.join(OUT, 'ARSetup.exe')


def main():
    if len(sys.argv) != 2 or not re.match(r'^v\d+\.\d+\.\d+$', sys.argv[1]):
        sys.exit(__doc__)
    version = sys.argv[1]
    os.makedirs(OUT, exist_ok=True)
    head = subprocess.run(['git', '-C', ROOT, 'rev-parse', '--short', 'HEAD'], capture_output=True, text=True, check=True).stdout.strip()
    if subprocess.run(['git', '-C', ROOT, 'diff', '--quiet', 'HEAD'], capture_output=True).returncode != 0:
        print('Note: there are uncommitted changes. They are left out; the zip has commit', head)

    setup = build_setup()
    path = os.path.join(OUT, f'Anarchy-Reigns-Supreme-{version}-Windows.zip')
    subprocess.run(['git', '-C', ROOT, 'archive', '--format=zip', '-o', path, 'HEAD'], check=True)
    with zipfile.ZipFile(path, 'a', zipfile.ZIP_DEFLATED) as z:
        z.write(setup, 'ARSetup.exe')

    with zipfile.ZipFile(path) as z:
        names = [n for n in z.namelist() if not n.endswith('/')]
    bad = [n for n in names if FORBIDDEN.search(n)]
    if bad:
        os.remove(path)
        sys.exit('Left out, as these must never be shared: ' + ', '.join(bad[:10]))
    for need in ('recomplauncher.json', 'ARSetup.exe', 'Setup.bat', 'ARRecompLib/config/AR.toml', 'thirdparty/XenonRecomp/XenonUtils/ppc_context.h'):
        if need not in names:
            os.remove(path)
            sys.exit(need + ' is missing from the zip.')

    digest = hashlib.sha256(open(path, 'rb').read()).hexdigest()
    print(f'{path}\n  commit {head}, {len(names)} files, {os.path.getsize(path) / 1e6:.1f} MB\n  sha256 {digest}')


main()
