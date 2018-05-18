{% target '/etc/sysconfig/network' %}#HEADER

NETWORKING=yes
HOSTNAME={{ hostname }}.{{ domain }}
{% endtarget %}
