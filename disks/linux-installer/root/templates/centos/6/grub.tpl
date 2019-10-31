{% target '/boot/grub/grub.conf' %}# Auto Generated During Install
{% if sol_console %}
terminal input console serial
terminal output console serial
serial --unit={{ sol_console|last }} --speed=115200
{% endif %}

default=0
timeout=10

title CentOS {{ distro_version|title }} - By Bootable-Disks
  root ({{ _installer.bootloader.grub_root }})
  kernel {{ _installer.bootloader.kernel_path }} ro root={{ _installer.filesystem.block_device }} {% if sol_console %}console={{ sol_console }},115200 console=tty1{% endif %}
  initrd {{ _installer.bootloader.initrd_path }}
{% endtarget %}
