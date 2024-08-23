{% target '/etc/yum.conf' %}# Auto Generated During Install

[main]
cachedir=/var/cache/yum/$basearch/$releasever
keepcache=1
debuglevel=2
errorlevel=2
logfile=/var/log/yum.log
exactarch=1
obsoletes=1
plugins=1
installonly_limit=5
distroverpkg=centos-release
http_caching=all
metadata_expire=6h
{% endtarget %}

