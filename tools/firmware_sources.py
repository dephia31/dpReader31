"""Read the single maintained firmware source, including owned edition fragments."""
from pathlib import Path
import re
ROOT=Path(__file__).resolve().parents[1]


def expand_fragments(text, root=ROOT):
    def expand(match):
        path=(Path(root)/match.group(1)).resolve()
        if not path.is_relative_to(Path(root).resolve()/'firmware'):
            raise ValueError('Edition fragment outside firmware/')
        return expand_fragments(path.read_text(),root)
    return re.sub(r'^\s*#include "(firmware/[^"\n]+\.inc)"\s*$',expand,text,flags=re.M)


def source_text(path):
    return expand_fragments(Path(path).read_text())


def legacy_text(text, owned, legacy, aliases, root=ROOT):
    """Rebase physical-owner relative includes for generated legacy host sources."""
    import os
    reverse={value:key for key,value in aliases.items()}
    def rebase(match):
        include=match.group(1)
        if not include.startswith('.'):
            return match.group(0)
        target=os.path.normpath(str(Path(owned).parent/include)).replace(os.sep,'/')
        if not (Path(root)/target).exists():
            return match.group(0)
        target=reverse.get(target,target)
        include=os.path.relpath(target,str(Path(legacy).parent)).replace(os.sep,'/')
        return match.group(0).replace('"'+match.group(1)+'"','"'+include+'"')
    return re.sub(r'^\s*#include "([^"\n]+)"',rebase,text,flags=re.M)
