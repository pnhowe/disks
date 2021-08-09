{% target '/etc/machine-id' %}
{{ '{:032x}'.format( structure_id ) }}
{% endtarget %}
