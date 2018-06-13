{% set _entries = {} %}
{% set _order = [] %}
{% for repo in repo_list %}
{% if repo.type == 'yum' %}
{% if repo.group %}{% set group = repo.group %}{% else %}{% set group = 'other' %}{% endif %}
{% if group in _entries %}
{% do _entries.__setitem__( group, _entries[ group ] + [ repo ] ) %}
{% else %}
{% do _order.append( group ) %}
{% do _entries.__setitem__( group, [ repo ] ) %}
{% endif %}
{% endif %}
{% endfor %}

{% for group in _order %}
{% target '/etc/yum.repos.d/' + group + '.repo' %}#HEADER

{% for repo in _entries[ group ] %}
[{{ repo.name }}]
name={{ distro }} - {{ distro_version }} - {{ group }} - {{ repo.name }}
baseurl={{ repo.uri }}
enabled=1
{% if repo.proxy and repo.proxy != 'local' %}proxy={{ repo.proxy }}{% endif %}
{% if repo.key_file %}gpgcheck=1
gpgkey=file://{{ repo.key_file }}{% endif %}
{% endfor %}
{% endtarget %}
{% endfor %}
