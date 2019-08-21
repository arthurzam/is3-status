#!/bin/bash

if ! lsmod | cut -f1 -d" " | grep -q mac80211_hwsim; then
    modprobe mac80211_hwsim radios=4
fi

for i in {1..3}; do
    hostapd <(sed hostapd.conf.in -e "s:@SSID@:Net${i}:g" -e "s:@INTERFACE@:wlan${i}:g") &
done

wpa_supplicant -Dnl80211 -iwlan0 -cwpa_supplicant.conf &
