#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Keep the offline transfer header logo byte-identical to the web asset."""
import base64
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]
asset=ROOT/'web/assets/dpreader-logo-red-studio.png'
page=ROOT/'components/read_pico_transfer/upload.html'
uri='data:image/png;base64,'+base64.b64encode(asset.read_bytes()).decode('ascii')
html=page.read_text()
pattern=r'(<img class="brand-logo" data-dpreader-logo src=")[^"]*(")'
html,count=re.subn(pattern,lambda match:match[1]+uri+match[2],html)
if count!=1:raise ValueError('Expected exactly one transfer logo slot')
page.write_text(html)
print('Synced offline logo:',asset.stat().st_size,'bytes; no external resource request')
