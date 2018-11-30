{% target '/etc/sysconfig/network' %}# Auto Generated During Install

NETWORKING=yes
HOSTNAME={{ _hostname }}.{{ domain_name }}
{% endtarget %}
