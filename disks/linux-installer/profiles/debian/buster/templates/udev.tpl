{% target '/etc/udev/rules.d/70-persistent-net.rules' %}# Auto Generated During Install

{% for interface_name in _interface_map %}{% set interface = _interface_map[ interface_name ] %}{% if interface.physical_location.startswith( 'eth' ) %}
{% if interface.mac %}SUBSYSTEM=="net", ACTION=="add", DRIVERS=="?*", ATTR{address}=="{{ interface.mac }}", ATTR{type}=="1", KERNEL=="e*", NAME="{{ interface_name }}"{% endif %}
{% endif %}{% endfor %}
{% endtarget %}
