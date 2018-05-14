{% target '/etc/selinux/config' %}#HEADER

SELINUX={{ selinux|default( 'permissive' ) }}
SELINUXTYPE={{ selinux_type|default( 'targeted' ) }}
{% endtarget %}
