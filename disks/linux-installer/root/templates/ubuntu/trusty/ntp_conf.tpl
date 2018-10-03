{% target '/etc/ntp.conf' %}# Auto Generated During Install

driftfile /var/lib/ntp/ntp.drift
statsdir /var/log/ntpstats/

statistics loopstats peerstats clockstats
filegen loopstats file loopstats type day enable
filegen peerstats file peerstats type day enable
filegen clockstats file clockstats type day enable

{% for server in ntp_servers %}
server {{ server }} iburst{% endfor %}

server 127.127.1.0
fudge 127.127.1.0 stratum 13
restrict default kod notrap nomodify nopeer noquery
restrict 127.0.0.1 nomodify
{% endtarget %}
