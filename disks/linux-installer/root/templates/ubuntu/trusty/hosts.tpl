{% target '/etc/hosts' %}# Auto Generated During Install

127.0.0.1 localhost
{% for interface in interface_list %}{% if interface.name != 'ipmi' %}{% for address in interface.address_list %}
{{ address.address }} {% if address.primary %}{{ hostname }}.{{ domain }} {{ hostname }} {% endif %}{% if address.vlan %}v{{ address.vlan }}.{{ interface.name }}{% if address.sub_interface %}-{{ address.sub_interface }}{% endif %}.{{ hostname }}.{{ domain }} {% endif %}{% if not address.tagged %}{{ interface.name }}{% if address.sub_interface %}-{{ address.sub_interface }}{% endif %}.{{ hostname }}.{{ domain }} {% endif %}{% endfor %}{% endif %}{% endfor %}

# The following lines are desirable for IPv6 capable hosts
::1     ip6-localhost ip6-loopback
fe00::0 ip6-localnet
ff00::0 ip6-mcastprefix
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters
ff02::3 ip6-allhosts
{% endtarget %}
