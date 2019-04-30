linux-installer
===============

teamplate values
----------------

  :image_package:
  :resource_location: (optional)
  distro
  distro_version
  bootstrap_source


  :/config.json: config vaules if local_config is specified, otherwise retrieved via the controller client


md_meta_version
target_drives
swap_size
boot_size
mounting_options
partition_scheme
partition_recipe
repo_list
root_pass
packages

  :postinstall_script: url to a script to download and executed in the target install, ie: `http://static/stuff.sh`
  :postinstall_commands: list of commands to run in the target install, ie: `[ 'echo "configured" > /etc/file', 'ls -l / > /stock_root' ]`
