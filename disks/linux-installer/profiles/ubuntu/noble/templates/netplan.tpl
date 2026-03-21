{% set global_ns = namespace( table_id=100 ) %}
{% set bonds = {} %}
{% set interface_name_list = [] %}
{% for interface_name in _interface_map %}{% do interface_name_list.append( interface_name ) %}{% endfor %}
{% for interface_name in interface_name_list %}
  {% if interface_name == 'ipmi' %}
    {% do _interface_map.__delitem__( interface_name ) %}
  {% else %}
    {% set interface = _interface_map[ interface_name ] %}
    {% if 'primary' in interface %}
      {% do bonds.update( { interface_name: interface } ) %}
    {% endif %}
  {% endif %}
{% endfor %}
{% set loop_type_list = [] %}
{% if _interface_map %}
  {% do loop_type_list.append( 'ethernets' ) %}
  {% if bonds %}
    {% do loop_type_list.append( 'bonds' ) %}
  {% endif %}
{% endif %}

{%- target '/etc/netplan/01-netcfg.yaml', 'root.root', '0600' -%}
# Auto Generated During Install

network:
  version: 2
  renderer: networkd
{%- for loop_type in loop_type_list %}
  {{ loop_type }}:
  {%- for interface_name in _interface_map -%}
    {%- if ( loop_type == 'bonds' and interface_name in bonds ) or ( loop_type != 'bonds' and interface_name not in bonds ) -%}
      {% set interface = _interface_map[ interface_name ] %}
    {{ interface_name }}:
      {%- if loop_type == 'ethernets' and interface.mac %}
      match:
        macaddress: {{ interface.mac }}
      set-name: {{ interface_name }}
      {%- endif %}
      {%- if loop_type == 'bonds' %}
      interfaces: [ {{ ', '.join( [ interface.primary ] + interface.secondary ) }} ]
        {%- if interface.paramaters %}
      parameters:
          {%- for key, value in interface.paramaters.items() %}
        {{ key }}: {{ value }}
          {%- endfor %}
        {%- endif %}
      {%- endif %}
      {%- if interface.mtu %}
      mtu: {{ interface.mtu }}
      {%- endif -%}
      {%- if interface.address_list %}
        {%- if interface.address_list[0].address == 'dhcp' %}
      dhcp4: yes
      dhcp6: no
        {%- else -%}
          {%- set addr_loop_ns = namespace( has_primary=false ) -%}
          {%- set route_list = [] -%}
          {%- for tmpaddr in interface.address_list -%}
            {%- for route in tmpaddr.route_list %}{% do route_list.append( route ) %}{% endfor -%}
            {%- if tmpaddr.primary %}{% set addr_loop_ns.has_primary = true %}{% endif -%}
            {%- if tmpaddr.gateway %}{% do route_list.append( dict( route='0.0.0.0/0', gateway=tmpaddr.gateway ) ) %}{% endif -%}
          {%- endfor -%}
          {%- set has_primary = addr_loop_ns.has_primary %}
      dhcp4: no
      dhcp6: no
      addresses: [ {% for tmpaddr in interface.address_list %}{{ tmpaddr.address }}/{{ tmpaddr.prefix }} {% endfor %} ]
          {% if route_list %}
      routes:
            {%- for route in route_list %}
        - to: {{ route.route }}
          via: {{ route.gateway }}
              {%- if route.table %}
          table: {{ route.table }}
              {%- endif -%}
            {%- endfor -%}
          {%- endif %}
          {%- if has_primary %}
      nameservers:
        search: [ {{ dns_search|join( ', ' ) }} ]
        addresses: [ {{ dns_servers|join( ', ' ) }} ]
          {%- endif -%}
        {%- endif -%}
      {%- else %}
      dhcp4: no
      dhcp6: no
      {%- endif -%}
    {%- endif -%}
  {%- endfor %}
{%- endfor %}
{% endtarget %}
