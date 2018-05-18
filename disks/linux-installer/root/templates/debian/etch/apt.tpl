{% set _host_list = [] %}
{% target '/etc/apt/apt.conf' %}//HEADER

{% for repo in repo_list %}{% if repo.type == 'apt' %}{% if repo.proxy %}{% if repo.proxy != 'local' %}{% set tmp = repo.uri.split( '/' )[2] %}{% if tmp not in _host_list %}
Acquire::http::Proxy::{{ tmp }} "{{ repo.proxy }}";
{% do _host_list.append( tmp ) %}
{% endif %}{% endif %}{% endif %}{% endif %}{% endfor %}
{% endtarget %}
