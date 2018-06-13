{% target '/etc/resolv.conf' %}#HEADER

domain {{ domain }}
search {{ dns_servers|join( ' ' ) }}
{% for server in dns_servers %}
nameserver {{ server }}{% endfor %}
{% endtarget %}
