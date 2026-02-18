{%- target '/var/lib/cloud/seed/nocloud-net/user-data' -%}
# Auto Generated During Install
hostname: {{ _hostname }}
fqdn: {{ _hostname }}.{{ _domain_name }}
manage_etc_hosts: true
preserve_hostname: true

# users/password
password: {{ root_password_hash }}
chpasswd:
  expire: False

# ssh
allow_public_ssh_keys: true
# TODO: this should be true
disable_root: false

{% endtarget %}
