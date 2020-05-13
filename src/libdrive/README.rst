to generate libdrve_h.py, libenclosure_h.py

install tools::

  sudo apt install libclang-5.0-dev

  pip3 install ctypeslib2

make sure the libraries are built:

  make

make a symlink for short version name::

  ln -s libdrive.so.3.0.0 libdrive.so.3
  ln -s libenclosure.so.3.0.0 libenclosure.so.3

generate the python header::

  ~/.local/bin/clang2py --clang-args="-I /usr/include/x86_64-linux-gnu -I /usr/lib/gcc/x86_64-linux-gnu/6/include/ -target x86_64" -t x86_64-Linux -kfs libdrive.h -l libdrive.so.3 > libdrive_h.py
  ~/.local/bin/clang2py --clang-args="-I /usr/include/x86_64-linux-gnu -I /usr/lib/gcc/x86_64-linux-gnu/6/include/ -target x86_64" -t x86_64-Linux -kfs libenclosure.h -l libenclosure.so.3 > libenclosure_h.py

clean up::

  rm libdrive.so.3
  rm libenclosure.so.3


Verbosity levels:

1 - User errors - unable to find device type things
2 - High Level Errors - generic "failed" stuff
3 - Detail Errors - low level errors
4 - debug - dump cdb etc
