{% target '/etc/machine-id' %}
{% if _structure_id %}
{{ '%032x' | format( _structure_id | int ) }}
{% else %}
00000000000000000000000000000000
{% endif %}
{% endtarget %}
