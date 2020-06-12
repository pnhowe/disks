#!/bin/sh

CMDLINE_LIST=()

CMDLINE_LIST+=( "dhcp-any	dhcp on all interfaces	interface=any no_config" )
CMDLINE_LIST+=( "dhcp-eth0	dhcp on eth0	interface=eth0 no_config" )
CMDLINE_LIST+=( "dhcp-eth1	dhcp on eth1	interface=eth1 no_config" )
CMDLINE_LIST+=( "dhcp-eth2	dhcp on eth2	interface=eth2 no_config" )
CMDLINE_LIST+=( "dhcp-eth3	dhcp on eth3	interface=eth3 no_config" )
CMDLINE_LIST+=( "manual	manual network configuration	ip_prompt no_config" )
CMDLINE_LIST+=( "no-net	no network configuration	no_ip no_config" )
