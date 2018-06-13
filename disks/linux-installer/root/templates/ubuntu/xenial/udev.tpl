{% target '/etc/udev/rules.d/70-persistent-net.rules' %}# Auto Generated During Install

{% for interface in interfaces %}{% if interface.startswith( 'eth' ) %}
{% if interfaces[interface].mac %}SUBSYSTEM=="net", ACTION=="add", DRIVERS=="?*", ATTR{address}=="{{ interfaces[interface].mac }}", ATTR{type}=="1", KERNEL=="e*", NAME="{{ interfaces[interface].name }}"{% endif %}
{% endif %}{% endfor %}
{% endtarget %}
