#!/usr/bin/env python3

from distutils.core import setup

setup( name='config-curator',
       description='Config Curator',
       author='Peter Howe',
       version='0.1',
       author_email='pnhowe@gmail.com',
       packages=[ 'config_curator', 'config_curator.contractor', 'config_curator.contractor.cinp', 'libconfig', 'libconfig.jinja2', 'libconfig.providers', 'libconfig.markupsafe' ],
       )
