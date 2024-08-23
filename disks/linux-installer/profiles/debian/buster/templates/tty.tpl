{% if not _console.startswith( 'ttyS' ) %}{% target '/etc/init/' + _console + '.conf' %}# Auto Generated During Install

# {{ _console }} - getty

start on stopped rc RUNLEVEL=[2345] and (
            not-container or
            container CONTAINER=lxc or
            container CONTAINER=lxc-libvirt)

stop on runlevel [!2345]

respawn
exec /sbin/getty -8 115200 {{ _console }}
{% endtarget %}{% endif %}
