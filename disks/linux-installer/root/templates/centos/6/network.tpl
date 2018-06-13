{% target '/etc/sysconfig/network' %}# Auto Generated During Install

NETWORKING=yes
HOSTNAME={{ hostname }}.{{ domain }}
{% endtarget %}
