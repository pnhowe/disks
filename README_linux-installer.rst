linux-installer
===============

teamplate values
----------------

  :resource_location: (optional)
  :distro:
  :distro_version:
  :bootstrap_source:
  :md_meta_version:
  :target_drives:
  :swap_size:
  :boot_size:
  :mounting_options:
  :partition_scheme:
  :partition_recipe:
  :repo_list: [ { distribution = 'stable', type = 'apt', uri = 'http://dl.google.com/linux/chrome/deb/', components = [ 'main' ], key_uri = 'https://dl-ssl.google.com/linux/linux_signing_key.pub', proxy = '{{ mirror_proxy|default( "local" ) }}' } ]
  :root_password_hash:  hashed root password
  :user_list: [ { name = 'jbuser', group = 'jbgroup', password_hash = '', sudo_list = [ 'ALL=(ALL) ALL' ] }  ]
  :package_list:  list of packages to install, each item can be prefixed with a 'apt:'. 'yum:', or 'zypper:' if that package name is specific to a package manager
  :postinstall_script: url to a script to download and executed in the target install, ie: `http://static/stuff.sh`
  :postinstall_commands: list of commands to run in the target install, ie: `[ 'echo "configured" > /etc/file', 'ls -l / > /stock_root' ]`


/config.json: config vaules if local_config is specified, otherwise retrieved via the contractor client

also template_url
