{% set global_ns = namespace( table_id=100 ) %}
{% set pre_process_ns = namespace( gw_count=0 ) %}
{% set bonds = {} %}
{% set interface_name_list = [] %}
{% for interface_name in _interface_map %}{% do interface_name_list.append( interface_name ) %}{% endfor %}
{% for interface_name in interface_name_list %}
  {% if interface_name == 'ipmi' %}
    {% do _interface_map.__delitem__( interface_name ) %}
  {% else %}
    {% set interface = _interface_map[ interface_name ] %}
    {% for tmpaddr in interface.address_list %}
      {% if tmpaddr.gateway %}{% set pre_process_ns.gw_count = pre_process_ns.gw_count + 1 %}{% endif %}
    {% endfor %}
    {% if 'primary' in interface %}
      {% do bonds.update( { interface_name: interface } ) %}
    {% endif %}
  {% endif %}
{% endfor %}
{% if pre_process_ns.gw_count > 1 %}
  {% set do_pbr = true %}
{% else %}
  {% set do_pbr = false %}
{% endif %}
{% set loop_type_list = [] %}
{% if _interface_map %}
  {% do loop_type_list.append( 'ethernets' ) %}
  {% if bonds %}
    {% do loop_type_list.append( 'bonds' ) %}
  {% endif %}
{% endif %}



{% target '/etc/cloudinit' %}# Auto Generated During Install
# network info
hostname: {{ _hostname }}
create_hostname_file: true
fqdn: {{ _hostname }}.{{ _domain_name }}
preserve_hostname: true

# users/password
password: {{ root_password_hash }}
chpasswd:
  expire: False

# ssh
allow_public_ssh_keys: true
# TODO: this should be true
disable_root: false

# network
network:
  version: 2
{%- for loop_type in loop_type_list %}
  {{ loop_type }}:
  {%- for interface_name in _interface_map -%}
    {%- if ( loop_type == 'bonds' and interface_name in bonds ) or ( loop_type != 'bonds' and interface_name not in bonds ) -%}
      {% set interface = _interface_map[ interface_name ] %}
    {{ interface_name }}:
      {%- if loop_type == 'ethernets' and interface.mac %}
      match:
        macaddress: '{{ interface.mac }}'
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
          {%- set gateway_list = [] -%}
          {%- for tmpaddr in interface.address_list -%}
            {%- for route in tmpaddr.route_list %}{% do route_list.append( route ) %}{% endfor -%}
            {%- if tmpaddr.primary %}{% set addr_loop_ns.has_primary = true %}{% endif -%}
            {%- do gateway_list.append( tmpaddr.gateway ) -%}
          {%- endfor -%}
          {%- if gateway_list|length == 1 -%}
            {% set default_gateway = gateway_list[0] -%}
          {%- else -%}
            {%- for gateway in gateway_list -%}
              {%- set route = dict( route='0.0.0.0/0', gateway=gateway ) -%}
              {%- do route_list.append( route ) -%}
            {%- endfor -%}
          {%- endif -%}
          {%- set has_primary = addr_loop_ns.has_primary %}
      dhcp4: no
      dhcp6: no
      addresses: [ {% for tmpaddr in interface.address_list %}{{ tmpaddr.address }}/{{ tmpaddr.prefix }} {% endfor %} ]
          {%- if has_primary -%}
            {%- if default_gateway %}
      gateway4: {{ default_gateway }}
            {%- endif -%}
          {%- endif -%}
          {%- if do_pbr -%}
            {%- if gateway_list %}
      routing-policy:
            {%- for tmpaddr in interface.address_list -%}
              {%- set global_ns.table_id = global_ns.table_id + 1 -%}
              {%- if tmpaddr.gateway %}
              {%- set route = dict( route=tmpaddr.subnet + '/' ~ tmpaddr.prefix, gateway=tmpaddr.gateway, table=global_ns.table_id ) -%}
              {%- do route_list.append( route ) %}
        - from: {{ tmpaddr.subnet }}/{{ tmpaddr.prefix }}
          table: {{ global_ns.table_id }}
                {%- endif -%}
              {%- endfor -%}
            {%- endif -%}
          {%- endif -%}
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

#

{% endtarget %}
