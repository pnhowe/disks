{% target '/etc/hosts' %}# Auto Generated During Install

127.0.0.1 localhost
{% for interface in interfaces %}{% if interface != 'ipmi' %}{% for address in interfaces[interface].address_list %}
{{ address.address }} {% if address.primary %}{{ hostname }}.{{ domain }} {{ hostname }} {% endif %}{% if address.vlan %}v{{ address.vlan }}.{{ interface }}{% if address.sub_interface %}-{{ address.sub_interface }}{% endif %}.{{ hostname }}.{{ domain }} {% endif %}{% if not address.tagged %}{{ interface }}{% if address.sub_interface %}-{{ address.sub_interface }}{% endif %}.{{ hostname }}.{{ domain }} {% endif %}{% endfor %}{% endif %}{% endfor %}

# The following lines are desirable for IPv6 capable hosts
::1     ip6-localhost ip6-loopback
fe00::0 ip6-localnet
ff00::0 ip6-mcastprefix
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters
ff02::3 ip6-allhosts
{% endtarget %}
