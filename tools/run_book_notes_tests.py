#!/usr/bin/env python3
"""Reproduce notes/dictionary and placement checks without an ESP-IDF SDK.
SPDX-License-Identifier: Apache-2.0
"""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
output=root/'build/notes-tests';output.mkdir(parents=True,exist_ok=True)
subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-D_DARWIN_C_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=undefined','-g','-I'+str(root/'main/book'),str(root/'tools/book_notes_host_test.c'),str(root/'main/book/book_annotations.c'),str(root/'main/book/book_dictionary.c'),str(root/'main/book/book_dict_file.c'),'-lz','-o',str(output/'storage-dictionary')],check=True)
subprocess.run([str(output/'storage-dictionary')],check=True)
subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-D_DARWIN_C_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'main/book'),'-include',str(root/'tools/notes_fatfs_shim.h'),'-Drename=notes_fatfs_rename','-c',str(root/'main/book/book_annotations.c'),'-o',str(output/'fatfs-storage.o')],check=True)
subprocess.run(['cc','-std=c11','-D_DEFAULT_SOURCE','-D_DARWIN_C_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=undefined','-I'+str(root/'main/book'),str(root/'tools/book_notes_host_test.c'),str(output/'fatfs-storage.o'),str(root/'main/book/book_dictionary.c'),str(root/'main/book/book_dict_file.c'),'-lz',str(root/'tools/notes_fatfs_shim.c'),'-o',str(output/'fatfs-storage')],check=True)
subprocess.run([str(output/'fatfs-storage')],check=True)
for name in ['book_selection_geometry_test.py','book_notes_ui_host_test.py','stardict_prepare_host_test.py','book_dict_compressed_host_test.py']:
    subprocess.run([sys.executable,str(root/'tools'/name)],check=True)
