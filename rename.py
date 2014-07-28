#! /usr/bin/env python

import re
import os
import filecmp
import shutil

files = [f for f in os.listdir('raw')]

for f in files:
	if f.startswith('FL_Headers_'):
		name = f[len('FL_Headers_'):]
	elif f.startswith('FL_Sources_'):
		name = f[len('FL_Sources_'):]
	else:
		continue

	if name.endswith('_h'):
		name = name[:-len('_h')] + '.h'
	elif name.endswith('_cpp'):
		name = name[:-len('_cpp')] + '.cpp'

	old = 'raw/' + f
	new = 'renamed/' + name

	if os.path.isfile(new):
		if filecmp.cmp(old, new):
			shutil.copy(old, new)
		else:
			print("File {} exists and differs from {}; skipping.").format(new, old)
