{% target '/etc/resolv.conf' %}# Auto Generated During Install

domain {{ domain }}
search {{ dns_servers|join( ' ' ) }}
{% for server in dns_servers %}
nameserver {{ server }}{% endfor %}
{% endtarget %}
