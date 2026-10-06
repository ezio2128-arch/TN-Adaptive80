"""Static checks, not a substitute for MSBuild/UWP manifest validation."""
from pathlib import Path
import xml.etree.ElementTree as ET
import hashlib,json
root=Path(__file__).resolve().parents[1]
files=list(root.glob('*.vcxproj'))+list((root/'GameBarWidget').glob('*.xaml'))+list((root/'GameBarWidget').glob('*.csproj'))+[root/'GameBarWidget/Package.appxmanifest']
for f in files: ET.parse(f)
for f in root.rglob('*.json'): json.loads(f.read_text())
for f in [root/'ThirdParty/PresentMon.exe']:
    assert f.read_bytes()[:2]==b'MZ'
    assert hashlib.sha256(f.read_bytes()).hexdigest()=='b2a706bc6ad475749e3b7e3409263aa1e6906d45bdcf993f6dbc0f660188f1af'
manifest=(root/'GameBarWidget/Package.appxmanifest').read_text()
for contract in ['microsoft.gameBarUIExtension','FramePulse.Bridge','windows.fullTrustProcess','runFullTrust','windows.startupTask']:
    assert contract in manifest,contract
for f in root.glob('*.vcxproj'):
    tree=ET.parse(f);ns={'m':'http://schemas.microsoft.com/developer/msbuild/2003'}
    for item in tree.findall('.//m:ClCompile',ns):
        if 'Include' in item.attrib: assert (root/item.attrib['Include']).is_file(),item.attrib['Include']
print(f'PASS XML well-formedness ({len(files)} files), JSON, project sources, contracts, pinned PE hash')
