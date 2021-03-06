#!/bin/bash

######### Cool things #########
    #colors
    reset="[0m"
    red="[1m[31m"
    green="[1m[32m"
    yellow="[1m[33m"
    orange="[0m[33m"
    blue="[1m[34m"
    default_color="$yellow"

    #show logs in colors
    info() {
        if [ $# = 1 ]; then
            echo -e "${green}$1${reset}" >/dev/tty
        else
            echo -e "$2$1${reset}" > /dev/tty
        fi
    }

######### FUNCTIONS #########
    #show how to use the script
    usage_and_quit() {
        info "Usage:\n\t$SAC_USAGE" $default_color
        if [ ! -z "$SAC_OPTIONS" ]; then
        info "Options:\n\t$SAC_OPTIONS" $default_color
        fi
        if [ ! -z "$SAC_EXAMPLE" ]; then
            info "Example:\n\t$SAC_EXAMPLE" $default_color
        fi
        info "Bybye everybody!"
        exit 1
    }

    #exiting after throwing an error
    error_and_usage_and_quit() {
        info "######## $1 ########" $red
        usage_and_quit
    }

    #exiting after throwing an error
    error_and_quit() {
        info "######## $1 ########" $red
        exit 2
    }

    is_package_installed() {
        type $1 &>/dev/null
        return $?
    }

    #ensure that a package ($1, first arg) is installed (optionnaly in $2)
    check_package_in_PATH() {
        msg="Please ensure you have installed AND added '$1' to your PATH variable"
        if [ $# -gt 1 ]; then
            msg+="(PATH should be '$2')"
        fi

        if [ $# = 3 ]; then
            is_package_installed $1
        else
            is_package_installed $1 || { info "$msg" $red; exit 3; }
        fi
    }

    check_package() {
        msg="Program '$1' is missing. Please ensure you have installed "
        if [ $# -gt 1 ]; then
            msg+="'$2'."
        else
            msg+="it."
        fi

        if [ $# = 3 ]; then
            is_package_installed $1
        else
            is_package_installed $1 || { info "$msg" $red; exit 3; }
        fi
    }

    check_environment_variable() {
        eval "PATH_V=\$$1"
        if [ -z "$PATH_V" ]; then
            info "You did NOT define environment variable '$1'. Please do it before continuing." $red;
            exit 4;
        fi
    }
