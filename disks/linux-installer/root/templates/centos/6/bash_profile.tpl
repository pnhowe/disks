{% target '/etc/profile.d/plato.sh' %}# Auto Generated During Install

export PLATO_POD={{ pod }}
export PS1='\[\033[1;37m\][{{ pod }}] \[\033[01;32m\]\u\[\033[01;34m\]@\[\033[01;31m\]\h\[\033[00;34m\]{\[\033[01;34m\]\W\[\033[00;34m\]}\[\033[01;32m\]:\[\033[00m\] '
{% endtarget %}
