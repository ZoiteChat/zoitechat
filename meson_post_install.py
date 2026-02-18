#!/usr/bin/env python3

import os
import sys
import subprocess
import shutil

prefix = os.environ.get('MESON_INSTALL_PREFIX', '/usr/local')
datadir = os.path.join(prefix, 'share')

# Packaging tools define DESTDIR and this isn't needed for them
if 'DESTDIR' not in os.environ:
    def run_if_available(command, *args):
        if shutil.which(command) is None:
            print(f'Skipping {command}: command not found')
            return

        subprocess.call([command, *args])

    print('Updating icon cache...')
    run_if_available('gtk-update-icon-cache', '-qtf',
                     os.path.join(datadir, 'icons', 'hicolor'))

    print('Updating desktop database...')
    run_if_available('update-desktop-database', '-q',
                     os.path.join(datadir, 'applications'))
