{% target '/etc/selinux/config' %}# Auto Generated During Install

SELINUX={{ selinux|default( 'permissive' ) }}
SELINUXTYPE={{ selinux_type|default( 'targeted' ) }}
{% endtarget %}
