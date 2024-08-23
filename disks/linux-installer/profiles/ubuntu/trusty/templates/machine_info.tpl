{% target '/etc/machine-info' %}PRETTY_HOSTNAME={{ _fqdn }}
ICON_NAME=computer
CHASSIS=server
DEPLOYMENT=
LOCATION="{{ _foundation_id }} in {{ _site }}"
SITE={{ _site }}
FOUNDATION={{ _foundation_id }}
STRUCTURE={{ _structure_id }}
CONFIG_ID="{{ _structure_config_uuid }}"
{% endtarget %}
