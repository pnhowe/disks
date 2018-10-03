{% for interface in interface_list %}{% if interface.name != 'ipmi' %}
{% set root_name = '/etc/sysconfig/network/ifcfg-' + interface %}
{% if interface.address_list %}
{% for address in interface.address_list %}
{% set file_name = root_name %}
{% if address.vlan and address.tagged %}{% set file_name = file_name + '.' + address.vlan|string %}{% endif %}
{% if address.sub_interface %}{% set file_name = file_name + ':' + address.sub_interface|string %}{% endif %}
{% target file_name %}# Auto gended during install
{% if interface.master_interface and not address.tagged and not address.sub_interface %}
BONDING_MASTER=yes
BONDING_MODULE_OPTS='{% if interface.bonding_paramaters.mode %}mode={{ interface.bonding_paramaters.mode }}{% endif %}{% if interface.bonding_paramaters.miimon %} miimon={{ interface.bonding_paramaters.miimon }}{% endif %}{% if interface.bonding_paramaters.downdelay %} downdelay={{ interface.bonding_paramaters.downdelay }}{% endif %}{% if interface.bonding_paramaters.updelay %} updelay={{ interface.bonding_paramaters.updelay }}{% endif %}{% if interface.bonding_paramaters.xmit_hash_policy %} xmit_hash_policy={{ interface.bonding_paramaters.xmit_hash_policy }}{% endif %}'

BONDING_SLAVE0={{ interface.master_interface }}
{% for slave in interface.slave_interfaces %}
BONDING_SLAVE{{ loop.index }}={{ slave }}
{% endfor %}
{% endif %}
STARTMODE={% if address.auto %}auto{% else %}manual{% endif %}
{% if address.address == 'dhcp' %}
BOOTPROTO=dhcp
{% else %}
BOOTPROTO=static
IPADDR={{ address.address }}
NETMASK={{ address.netmask }}
{% endif %}
{% if address.vlan and address.tagged %}
VLAN=yes
ETHERDEVICE={{ interface.name }}
{% endif %}
MTU={{ address.mtu }}
{% endtarget %}
{% endfor %}
{% else %}
{% target root_name %}# Auto gended during install
BOOTPROTO=none
STARTMODE=off
{% endtarget %}
{% endif %}
{% endif %}{% endfor %}
