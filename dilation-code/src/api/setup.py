from distutils.core import setup, Extension
setup(name='timekeeper_functions', version='1.0',  \
      ext_modules=[Extension('timekeeper_functions', include_dirs=['/usr/local/include','/usr/local/include/python3.2'], library_dirs = ['/usr/local/lib','/usr/lib/i386-linux-gnu'], sources = ['py_extensions.c', 'TimeKeeper_functions.c', 'utility_functions.c'])])
