{% target '/etc/sysconfig/network/routes' %}# Auto gended during install
# Destination     Dummy/Gateway     Netmask            Device
#
127.0.0.0/24         0.0.0.0           -      lo
{% for interface in interfaces %}{% if interface != 'ipmi' %}{% for address in interfaces[interface].address_list %}
{% if address.gateway %}default {{ address.gateway }} - -{% endif %}
{% for route in address.route_list %}{{ route.route }} {{ route.gateway }} - {{ interface }}
{% endfor %}{% endfor %}{% endif %}{% endfor %}
{% endtarget %}
