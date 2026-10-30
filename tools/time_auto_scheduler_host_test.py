#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""Reuse the existing real scheduler fixture independently of board PMU fixtures."""
from pathlib import Path
import ast,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
module=ast.parse((root/'tools/time_service_host_test.py').read_text())
nodes=[]
for n in module.body:
 if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id in ('common','scheduler') for t in n.targets):nodes.append(n)
 elif isinstance(n,ast.AugAssign) and isinstance(n.target,ast.Name) and n.target.id=='scheduler':nodes.append(n)
env={'root':root};exec(compile(ast.Module(body=nodes,type_ignores=[]),'scheduler-fixture','exec'),env)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(env['scheduler'].replace('assert(!strcmp(ns,"kiiko_clock"))','assert(!strcmp(ns,KIIKO_BOARD_METALIO47?"mt_clock":"kiiko_clock"))'))
 for board in (0,1):
  subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-unused-variable','-fsanitize=undefined',f'-DKIIKO_BOARD_METALIO47={board}',str(p/'test.c'),'-o',str(p/'test')],check=True)
  subprocess.run([str(p/'test')],check=True)
