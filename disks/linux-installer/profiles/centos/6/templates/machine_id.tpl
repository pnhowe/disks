{% target '/etc/machine-id' %}
{{ '{:032x}'.format( _structure_id ) }}
{% endtarget %}
