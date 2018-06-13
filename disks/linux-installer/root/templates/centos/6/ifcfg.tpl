{% for interface in interfaces %}
{% for address in interfaces[interface].address_list %}
{% set filename = 'ifcfg-' + interfaces[interface].name %}
{% if address.vlan and address.tagged %}
{% set filename = filename + '.' + address.vlan|string %}
{% endif %}
{% if address.sub_interface %}
{% set filename = filename + ':' + address.sub_interface|string %}
{% endif %}
{% target '/etc/sysconfig/network-scripts/' + filename %}# Auto Generated During Install

{% if interfaces[interface].master_interface and not address.vlan and not address.tagged and not address.sub_interface %}
BONDING_MASTER=yes
BONDING_MODULE_OPTS='{% if interfaces[interface].bonding_paramaters.mode %}mode={{ interfaces[interface].bonding_paramaters.mode }}{% endif %}{% if interfaces[interface].bonding_paramaters.miimon %} miimon={{ interfaces[interface].bonding_paramaters.miimon }}{% endif %}{% if interfaces[interface].bonding_paramaters.downdelay %} downdelay={{ interfaces[interface].bonding_paramaters.downdelay }}{% endif %}{% if interfaces[interface].bonding_paramaters.updelay %} updelay={{ interfaces[interface].bonding_paramaters.updelay }}{% endif %}'
BONDING_SLAVE0={{ name_map[interfaces[interface].master_interface] }}
{% for slave in interfaces[interface].slave_interfaces %}
BONDING_SLAVE{{ loop.index }}={{ name_map[slave] }}
{% endfor %}
{% endif %}

DEVICE="{{ interfaces[interface].name }}"
ONBOOT="{% if address.auto %}yes{% else %}no{% endif %}"
TYPE="Ethernet"
HWADDR="{{ interfaces[interface].mac }}"
{% if address.address == 'dhcp' %}BOOTPROTO="dhcp"{% else %}BOOTPROTO="none"
IPADDR="{{ address.address }}"
NETMASK="{{ address.netmask }}"
NETWORK="{{ address.network }}"
GATEWAY="{{ address.gateway }}"
{% endif %}
{% if address.vlan and address.tagged %}VLAN="yes"{% endif %}
MTU="{{ address.mtu }}"
{% endtarget %}
{% endfor %}
{% endfor %}
