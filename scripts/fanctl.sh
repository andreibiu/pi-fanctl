#!/bin/bash


function stop-sleep() {
    kill -SIGINT $1;
}

function start-sleep() {
    sleep $1 &
    trap "stop-sleep $!" SIGINT
    wait
    trap - SIGINT
}

function get-pi-kernel-version-compile-define() {
    kernel_info=($(uname -r | tr '.+-' ' '))
    if [ "${kernel_info[0]}" -ge 6 ] && [ "${kernel_info[1]}" -ge 6 ] && [ "${kernel_info[2]}" -ge 28 ] && \
            [ "${kernel_info[3]}" == "rpt" ] && [ "${kernel_info[4]}" == "rpi" ]; then
        echo "KERN_VER_GE_6_6_28_RPI=1"
    else
        echo ""
    fi
}


DRIVER_NAME=$(basename $0) && export DRIVER_NAME=${DRIVER_NAME%.*}
BUILTIN_DRIVER_NAME="pwm_fan"

PI_5_DT_NAME="bcm2712-rpi-5-b"
PI_5_DTB_PATH="/boot/firmware/$PI_5_DT_NAME.dtb"
PI_KERN_VERSION_COMPILE_DEFINE=$(get-pi-kernel-version-compile-define)

INSTALL_DRIVER_EXTRAS_BASE_PATH="/usr/local/share/fanctl"
INSTALL_DRIVER_BIN_PATH="/usr/local/bin/$DRIVER_NAME"
INSTALL_DRIVER_MODULE_PATH="/lib/modules/$(uname -r)/extra/$DRIVER_NAME.ko.xz"
INSTALL_DRIVER_OVERLAY_PATH="/boot/overlays/$DRIVER_NAME.dtbo"
INSTALL_DRIVER_CONFIG_PATH="/usr/local/etc/$DRIVER_NAME.cfg"

read -r -d '' HELP <<- END
$(basename $0) [OPTION]
OPTION:
    -i | --install      Compiles the kernel module and installs the driver
    -u | --uninstall    Removes the driver and its additional files
    -c | --configure    Applies the new configuration from 'fanctl.cfg'
    -e | --enable       Enables the driver
    -d | --disable      Disables the driver
    -t | --test         Compiles the kernel module in debug mode and runs a simple test (requires stress-ng to be installed)
    -h | --help         Prints this help message (default action)
END

IS_SOURCE_DIR=$([[ "$(basename $(pwd))" == "pi-fanctl" ]] && echo 1 || echo 0)
if [ $IS_SOURCE_DIR -eq 1 ]; then
    export DRIVER_CONFIG_PATH="config/$DRIVER_NAME.cfg"
    export DRIVER_DTS_PATH="build/$DRIVER_NAME.dts"
    export DRIVER_DTS_TEMPLATE_BASE_PATH="templates"
    export PI_5_DTS_PATH="build/$PI_5_DT_NAME.dts"
else
    export DRIVER_CONFIG_PATH="$INSTALL_DRIVER_CONFIG_PATH"
    export DRIVER_DTS_PATH="/tmp/$DRIVER_NAME.dts"
    export DRIVER_DTS_TEMPLATE_BASE_PATH="$INSTALL_DRIVER_EXTRAS_BASE_PATH"
fi


if [ $EUID -ne 0 ]; then
    echo "$(basename $0) requires root privileges."
    exit -1
fi

if [ $# -gt 1 ]; then
    echo "Unknown argument(s)."
elif [ $# -eq 0 ] || [ $1 == "-h" ] || [ $1 == "--help" ]; then
    echo "$HELP"
elif [ $1 == "-i" ] || [ $1 == "--install" ]; then
    if [ $IS_SOURCE_DIR -ne 1 ]; then
        echo "Install command '$(basename $0)' should be executed from project directory."
        exit -1
    fi

    mkdir -p ./build

    if [ -f "$PI_5_DTB_PATH" ]; then
        dtc -q -I dtb -O dts $PI_5_DTB_PATH -o $PI_5_DTS_PATH
    fi

    python3 "scripts/${DRIVER_NAME}_config.py" driver_dts board_dts && \
    make $PI_KERN_VERSION_COMPILE_DEFINE build && make install && \
    python3 "scripts/${DRIVER_NAME}_config.py" boot_add
    if [ $? -ne 0 ]; then
        EXIT_CODE=$?
        make clean
        exit $EXIT_CODE
    fi

    if [ -f "$PI_5_DTB_PATH" ]; then
        if ! [ -f "$PI_5_DTB_PATH.bak" ]; then 
            mv $PI_5_DTB_PATH "$PI_5_DTB_PATH.bak"
        fi
        dtc -q -I dts -O dtb $PI_5_DTS_PATH -o $PI_5_DTB_PATH
    fi

    make clean

    mkdir -p $INSTALL_DRIVER_EXTRAS_BASE_PATH
    cp "scripts/$DRIVER_NAME.sh" $INSTALL_DRIVER_BIN_PATH
    cp "scripts/${DRIVER_NAME}_config.py" $INSTALL_DRIVER_EXTRAS_BASE_PATH
    cp templates/* $INSTALL_DRIVER_EXTRAS_BASE_PATH
    cp $DRIVER_CONFIG_PATH $INSTALL_DRIVER_CONFIG_PATH

    echo "The driver is now installed; press enter to reboot the system..."
    read && sleep 5 && reboot now
elif [ $1 == "-u" ] || [ $1 == "--uninstall" ]; then
    rmmod $DRIVER_NAME.ko
    python3 "$INSTALL_DRIVER_EXTRAS_BASE_PATH/${DRIVER_NAME}_config.py" boot_remove

    if [ -f "$PI_5_DTB_PATH.bak" ]; then
        mv -f "$PI_5_DTB_PATH.bak" $PI_5_DTB_PATH
    fi

    rm $INSTALL_DRIVER_MODULE_PATH
    rm $INSTALL_DRIVER_BIN_PATH
    rm $INSTALL_DRIVER_OVERLAY_PATH
    rm $INSTALL_DRIVER_CONFIG_PATH
    rm -rf $INSTALL_DRIVER_EXTRAS_BASE_PATH
    depmod -a

    echo "The driver is now uninstalled; press enter to reboot the system..."
    read && sleep 5 && reboot now
elif [ $1 == "-c" ] || [ $1 == "--configure" ]; then
    python3 "$INSTALL_DRIVER_EXTRAS_BASE_PATH/${DRIVER_NAME}_config.py" driver_dts || (exit $?)
    dtc -@ -I dts -O dtb -o $INSTALL_DRIVER_OVERLAY_PATH $DRIVER_DTS_PATH
    rm $DRIVER_DTS_PATH

    echo "Driver configuration is now applied; press enter to reboot the system..."
    read && sleep 5 && reboot now
elif [ $1 == "-e" ] || [ $1 == "--enable" ]; then
    python3 "$INSTALL_DRIVER_EXTRAS_BASE_PATH/${DRIVER_NAME}_config.py" boot_add
    echo "Driver is now enabled; press enter to reboot the system..."
    read && sleep 5 && reboot now
elif [ $1 == "-d" ] || [ $1 == "--disable" ]; then
    python3 "$INSTALL_DRIVER_EXTRAS_BASE_PATH/${DRIVER_NAME}_config.py" boot_remove
    echo "Driver is now disabled; press enter to reboot the system..."
    read && sleep 5 && reboot now
elif [ $1 == "-t" ] || [ $1 == "--test" ]; then
    if [ $IS_SOURCE_DIR -ne 1 ]; then
        echo "Test command '$(basename $0)' should be executed from project directory."
        exit -1
    fi

    mkdir -p ./build

    python3 "scripts/${DRIVER_NAME}_config.py" driver_dts && \
    make $PI_KERN_VERSION_COMPILE_DEFINE DEBUG=1 build && make install_dtb
    if [ $? -ne 0 ]; then
        EXIT_CODE=$?
        make clean
        exit $EXIT_CODE
    fi

    BUILTIN_DRIVER_LOADED=$(lsmod | grep $BUILTIN_DRIVER_NAME)
    if ! [ -z "$BUILTIN_DRIVER_LOADED" ]; then rmmod $BUILTIN_DRIVER_NAME; fi
    dtoverlay $DRIVER_NAME && insmod "build/$DRIVER_NAME.ko"

    stress-ng --cpu-method fft --aggressive --cpu 1 --timeout 90s
    start-sleep 90
    stress-ng --cpu-method fft --aggressive --cpu 16 --timeout 60s
    start-sleep 60

    rmmod $DRIVER_NAME.ko; dtoverlay -r $DRIVER_NAME
    if ! [ -z "$BUILTIN_DRIVER_LOADED" ]; then modprobe $BUILTIN_DRIVER_NAME; fi

    make clean
    rm $INSTALL_DRIVER_OVERLAY_PATH
else
    echo "Unknown argument(s)."
fi
