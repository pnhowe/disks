{% target '/boot/grub/grub.conf' %}# Auto Generated During Install
{% if _console.startswith("ttyS") %}
terminal input console serial
terminal output console serial
serial --unit={{ _console|last }} --speed=115200
{% endif %}

default=0
timeout=10

title CentOS {{ distro_version|title }} - By T3kton-Disks
  root ({{ _installer.bootloader.grub_root }})
  kernel {{ _installer.bootloader.kernel_path }} ro root={{ _installer.filesystem.block_device }}{% if _console.startswith("ttyS") %} console={{ _console }},115200 console=tty1{% endif %}
  initrd {{ _installer.bootloader.initrd_path }}
{% endtarget %}
