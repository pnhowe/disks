{% target '/etc/netplan/20-installer.yaml' %}# Auto Generated During Install

network:
  version: 2
  renderer: networkd
  ethernets:
{% for interface in _foundation_interface_list %}{% if interface != 'ipmi' %}{% if interface.address_list %}
    {{ interface.name }}:
      match:
        macaddress: {{ interface.mac }}
      set-name: {{ interface.name }}
{% for address in interface.address_list %}{% if address.primary %}
      mtu: {{ address.mtu }}
{% if address.address == 'dhcp' %}
      dhcp4: yes
      dhcp6: no
{% else %}
      dhcp4: no
      dhcp6: no
      addresses: [ {{ address.address }}/{{ address.prefix }} ]
{% if address.gateway %}      gateway4: {{ address.gateway }}{% endif %}
{% if address.primary %}      nameservers:
        search: [{{ dns_search|join( ', ' ) }}]
        addresses: [{{ dns_servers|join( ', ' ) }}] {% endif %}
{% endif %}
{% endif %}{% endfor %}
{% endif %}{% endif %}{% endfor %}
{% endtarget %}
