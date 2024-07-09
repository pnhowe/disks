ttytee
======

ttytee will "tee" tty devices.  It is used here to duplicate the console
of the PXE disks for cases where there is a Serial Over Lan(SOL).  This
way the PXE disks can be interacted with both via the physical monitor
as well as the SOL.

Usage
-----

before use you must start ttytee::

  /sbin/ttytee -d /dev/mastertty /dev/tty1 /dev/ttyS1

the "-d" will tell ttytee to damonize/background it's self.

This will start up a new tty called "/dev/mastertty" that will be forwared
to both "/dev/tty1" and "/dev/ttyS1".

NOTE: use the value "-" to add your curent tty as one of the ttys to use.

if you would also like to save the output to a log file, add a path to a non existant file::

  /sbin/ttytee -d /dev/mastertty /dev/tty1 /dev/ttyS1 /tmp/output.log

NOTE: there is not log rotating, and is not any checking for filesystem
limits.  If you were to want to rotate the log

Then you can start up a process using "/dev/mastertty".  This is an example
for a busybox /etc/inittab file::

  mastertty::once:/etc/init.d/do_task
