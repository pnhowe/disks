{% for interface_name, interface in _interface_map.items() %}
# on network {{ interface.network }}
{% for address in interface.address_list %}
{% set filename = 'ifcfg-' + interface_name %}
{% if address.vlan %}
{% set filename = filename + '.' + address.vlan|string %}
{% endif %}
{% if address.alias_index %}
{% set filename = filename + ':' + address.alias_index|string %}
{% endif %}
{% target '/etc/sysconfig/network-scripts/' + filename %}# Auto Generated During Install

{% if interface.master_interface and not address.vlan and not address.alias_index %}
BONDING_MASTER=yes
BONDING_MODULE_OPTS='{% if interface.bonding_paramaters.mode %}mode={{ interface.bonding_paramaters.mode }}{% endif %}{% if interface.bonding_paramaters.miimon %} miimon={{ interface.bonding_paramaters.miimon }}{% endif %}{% if interface.bonding_paramaters.downdelay %} downdelay={{ interface.bonding_paramaters.downdelay }}{% endif %}{% if interface.bonding_paramaters.updelay %} updelay={{ interface.bonding_paramaters.updelay }}{% endif %}'
BONDING_SLAVE0={{ name_map[interface.master_interface] }}
{% for slave in interface.slave_interfaces %}
BONDING_SLAVE{{ loop.index }}={{ name_map[slave] }}
{% endfor %}
{% endif %}

DEVICE="{{ interface_name }}"
ONBOOT="{% if address.auto %}yes{% else %}no{% endif %}"
TYPE="Ethernet"
HWADDR="{{ interface.mac }}"
{% if address.address == 'dhcp' %}BOOTPROTO="dhcp"{% else %}BOOTPROTO="none"
IPADDR="{{ address.address }}"
NETMASK="{{ address.netmask }}"
NETWORK="{{ address.subnet }}"
GATEWAY="{{ address.gateway }}"
{% endif %}
{% for server in dns_servers %}
DNS{{ loop.index }}="{{ server }}"
{% endfor %}
DOMAIN="{{ dns_search|join( ' ' ) }}"
{% if address.vlan %}VLAN="yes"{% endif %}
{% if interface.mtu %}MTU="{{ interface.mtu }}"{% endif %}
{% endtarget %}
{% endfor %}
{% endfor %}
