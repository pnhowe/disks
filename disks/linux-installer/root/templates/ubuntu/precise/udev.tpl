{% target '/etc/udev/rules.d/70-persistent-net.rules' %}# Auto Generated During Install

{% for interface in _foundation_interface_list %}{% if interface.name.startswith( 'eth' ) %}
{% if interface.mac %}SUBSYSTEM=="net", ACTION=="add", DRIVERS=="?*", ATTR{address}=="{{ interface.mac }}", ATTR{type}=="1", KERNEL=="e*", NAME="{{ interface.name }}"{% endif %}
{% endif %}{% endfor %}
{% endtarget %}
