{% target '/etc/default/grub' %}# Auto Generated During Install

GRUB_DEFAULT=0
#GRUB_HIDDEN_TIMEOUT=0
GRUB_HIDDEN_TIMEOUT_QUIET=true
GRUB_TIMEOUT=5
GRUB_DISTRIBUTOR="CentOS {{ distro_version|title }} - By T3kton-Disks"
GRUB_CMDLINE_LINUX_DEFAULT=""
GRUB_CMDLINE_LINUX="{% if _console.startswith("ttyS") %} console={{ _console }},115200 console=tty1{% endif %} crashkernel=auto"
GRUB_DISABLE_RECOVERY="true"

{% if _console.startswith("ttyS") %}
GRUB_TERMINAL="console serial"
GRUB_SERIAL_COMMAND="serial --unit={{ _console|last }} --speed=115200"
{% else %}
GRUB_TERMINAL="console"
{% endif %}

{% endtarget %}
