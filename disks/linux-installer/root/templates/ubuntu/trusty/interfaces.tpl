{% set _hide_interfaces = [] %}
{% target '/etc/network/interfaces' %}# Auto Generated During Install

auto lo
iface lo inet loopback

{% for interface_name in _interface_map %}{% if interface_name != 'ipmi' %}
{% set interface = _interface_map[ interface_name ] %}
{% if interface.address_list %}
{% set ifname = interface_name %}
{% for address in interface.address_list %}
{% if address.vlan and address.tagged %}{% set ifname = ifname + "." + address.vlan|string %}{% endif %}
{% if address.sub_interface %}{% set ifname = ifname + ":" + address.sub_interface %}{% endif %}
{% if address.auto %}auto {{ ifname }}{% endif %}
{% if address.address == 'dhcp' %}
iface {{ ifname }} inet dhcp
{% else %}
iface {{ ifname }} inet static
  address {{ address.address }}
  netmask {{ address.netmask }}
{% if address.network %}  network {{ address.network }}{% endif %}
  mtu {{ address.mtu }}
{% if address.vlan and address.tagged and address_primary.gateway %}{% if address.gateway %}  up ip rule add from {{ address.address }} table {{ address.vlan }}
  up ip rule add from {{ address.address }} to {{ address.network }}/{{ address.prefix }} table main
  up ip route add default via {{ address.gateway }} table {{ address.vlan }}
  down ip route del default via {{ address.gateway }} table {{ address.vlan }}
  down ip rule del from {{ address.address }} to {{ address.network }}/{{ address.prefix }} table main
  down ip rule del from {{ address.address }} table {{ address.vlan }}{% else %}
  up ip rule add from {{ address_primary.address }} to {{ address.network }}/{{ address.prefix }} lookup {{ address.vlan }}
  up ip route add default via {{ address_primary.gateway }} table {{ address.vlan }}
  down ip route del default via {{ address_primary.gateway }} table {{ address.vlan }}
  down ip rule del from {{ address_primary.address }} to {{ address.network }}/{{ address.prefix }} lookup {{ address.vlan }}{% endif %}
{% else %}{% if address.gateway %}  gateway {{ address.gateway }}{% endif %}{% endif %}
{% for route in address.route_list %}
  up ip route add {{ route.route }} via {{ route.gateway }}
  down ip route del {{ route.route }} via {{ route.gateway }}{% endfor %}
{% if address.primary %}  dns-nameservers {{ dns_servers|join( ' ' ) }}
  dns-search {{ dns_search|join( ' ' ) }}{% endif %}
{% endif %}
{% if interface.master_interface and not address.tagged and not address.sub_interface %}
{% do _hide_interfaces.append( interface.master_interface ) %}{% for tmp in interface.slave_interfaces %}{% do _hide_interfaces.append( tmp ) %}{% endfor %}
  bond-slaves {{ interface.master_interface }} {{ interface.slave_interfaces|join( ' ' ) }}
{% if interface.bonding_paramaters.mode %}  bond-mode {{ interface.bonding_paramaters.mode }}{% endif %}
{% if interface.bonding_paramaters.miimon %}  bond-miimon {{ interface.bonding_paramaters.miimon }}{% endif %}
{% if interface.bonding_paramaters.downdelay %}  bond-downdelay {{ interface.bonding_paramaters.downdelay }}{% endif %}
{% if interface.bonding_paramaters.updelay %}  bond-updelay {{ interface.bonding_paramaters.updelay }}{% endif %}
{% if interface.bonding_paramaters.xmit_hash_policy %}  bond-xmit_hash_policy {{ interface.bonding_paramaters.xmit_hash_policy }}{% endif %}
auto {{ interface.master_interface }}
iface {{ interface.master_interface }} inet manual
  bond-master {{ interface }}
{% for slave in interface.slave_interfaces %}
auto {{ slave }}
iface {{ slave }} inet manual
  bond-master {{ interface }}
{% endfor %}
{% endif %}

{% endfor %}
{% elif interface not in _hide_interfaces %}
iface {{ interface }} inet manual
{% endif %}{% endif %}{% endfor %}
{% endtarget %}
