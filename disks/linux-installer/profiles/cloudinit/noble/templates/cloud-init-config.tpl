{%- target '/etc/cloud/cloud.cfg.d/99-contractor-nocloud.cfg' -%}
# Contractor cloud-init Configuration

datasource_list: [ NoCloud, None ]

datasource:
  NoCloud:
    seedfrom: file:///var/lib/cloud/seed/nocloud-net/

{% endtarget %}
