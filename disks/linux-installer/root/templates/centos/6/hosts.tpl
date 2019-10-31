{% target '/etc/hosts' %}# Auto Generated During Install

127.0.0.1 localhost
{% for interface_name in _interface_map %}{% if interface_name != 'ipmi' %}{% set interface = _interface_map[ interface_name ] %}{% for address in interface.address_list %}
{{ address.address }} {% if address.primary %}{{ _hostname }}.{{ domain_name }} {{ _hostname }} {% endif %}{% if address.vlan %}v{{ address.vlan }}.{{ interface.name }}{% if address.sub_interface %}-{{ address.sub_interface }}{% endif %}.{{ _hostname }}.{{ domain_name }} {% endif %}{% if not address.tagged %}{{ interface.name }}{% if address.sub_interface %}-{{ address.sub_interface }}{% endif %}.{{ _hostname }}.{{ domain_name }} {% endif %}{% endfor %}{% endif %}{% endfor %}

# The following lines are desirable for IPv6 capable hosts
::1     ip6-localhost ip6-loopback
fe00::0 ip6-localnet
ff00::0 ip6-mcastprefix
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters
ff02::3 ip6-allhosts
{% endtarget %}
