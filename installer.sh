#!/usr/bin/bash

if [ $# -eq 0 ] || [ $1 == "-i" ] || [ $1 == "--install" ]; then
    make build
    mv fanctl /usr/local/bin/fanctl
    cp fanctl.service /etc/systemd/system/fanctl.service
    systemctl enable fanctl
    systemctl start fanctl
elif [ $1 == "-u" ] || [ $1 == "--uninstall" ]; then
    systemctl stop fanctl
    systemctl disable fanctl
    rm /etc/systemd/system/fanctl.service
    rm /usr/local/bin/fanctl
else
    echo "Unknown argument(s)."
fi
