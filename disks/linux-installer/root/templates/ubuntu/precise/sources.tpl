{% set _entries = {} %}
{% set _order = [] %}
{% for repo in repo_list %}
{% if repo.type == 'apt' %}
{% set tmp = ( repo.uri, repo.distribution, repo.options ) %}
{% if tmp in _entries %}
{% do _entries.__setitem__( tmp, _entries[ tmp ] + repo.components ) %}
{% else %}
{% do _order.append( tmp ) %}
{% do _entries.__setitem__( tmp, repo.components ) %}
{% endif %}
{% endif %}
{% endfor %}

{% target '/etc/apt/sources.list' %}#HEADER

{% for tmp in _order %}
deb {% if tmp[2] %}[{{ tmp[2] }}] {% endif %}{{ tmp[0] }} {{ tmp[1] }}{% for component in _entries[ tmp ]|unique_list %} {{ component }}{% endfor %}
{% endfor %}
{% endtarget %}
