{% target '/etc/profile.d/site.sh' %}# Auto Generated During Install

{% if site %}
export CONTRACTOR_SITE={{ site }}
export PS1='\[\033[1;37m\][{{ site }}] \[\033[01;32m\]\u\[\033[01;34m\]@\[\033[01;31m\]\h\[\033[00;34m\]{\[\033[01;34m\]\W\[\033[00;34m\]}\[\033[01;32m\]:\[\033[00m\] '
{% else %}
export PS1='\[\033[1;37m\]\[\033[01;32m\]\u\[\033[01;34m\]@\[\033[01;31m\]\h\[\033[00;34m\]{\[\033[01;34m\]\W\[\033[00;34m\]}\[\033[01;32m\]:\[\033[00m\] '
{% endif %}
{% endtarget %}
