#!/bin/bash

#how to use the script
export PLATFORM_OPTIONS="\
\t-a|-atlas: generate all atlas from unprepared_assets directory
\t-g|-gradle: use gradle(android studio) instead of ant(eclipse)
\t-p|-project: regenerate the ant files needed for build
\t-i|-install: install on device
\t-l|-log: run adb logcat
\t-s|-stacktrace: show latest dump crash trace"


parse_arguments() {
    GENERATE_ATLAS=0
    USE_GRADLE=0
    REGENERATE_ANT_FILES=0
    INSTALL_ON_DEVICE=0
    RUN_LOGCAT=0
    STACK_TRACE=0
    while [ "$1" != "" ]; do
        case $1 in
            #ignore higher level commands
            "-h" | "-help" | "-release" | "-debug")
                # info "Ignoring '$1' (low lvl)"
                ;;
            #ignore higher level commands
            "--c" | "--cmakeconfig" | "--t" | "--target")
                # info "Ignoring '$1' and its arg '$2' (low lvl)"
                shift
                ;;
            "-a" | "-atlas")
                GENERATE_ATLAS=1
                TARGETS=$TARGETS" "
                ;;
            "-g" | "-gradle")
                USE_GRADLE=1
                TARGETS=$TARGETS" "
                ;;
            "-p" | "-project")
                REGENERATE_ANT_FILES=1
                TARGETS=$TARGETS"n"
                ;;
            "-i" | "-install")
                INSTALL_ON_DEVICE=1
                TARGETS=$TARGETS" "
                ;;
            "-l" | "-log")
                RUN_LOGCAT=1
                TARGETS=$TARGETS" "
                ;;
            "-s" | "-stacktrace")
                TARGETS=$TARGETS" "
                STACK_TRACE=1
                ;;
            --*)
                info "Unknown option '$1', ignoring it and its arg '$2'" $red
                shift
                ;;
            -*)
                info "Unknown option '$1', ignoring it" $red
                ;;
        esac
        shift
    done
}

check_necessary() {
    #test that the path contains android SDK and android NDK as required
	check_package_in_PATH "android" "android-sdk/tools"
    check_package_in_PATH "ndk-build" "android-ndk"
	check_package_in_PATH "adb" "android-sdk/platform-tools"
	check_package "ant"
}

compilation_before() {
    if [ $GENERATE_ATLAS = 1 ]; then
        info "Generating atlas..."
        $rootPath/sac/tools/generate_atlas.sh $rootPath/unprepared_assets/*
    fi

	info "Building tremor lib..."
	cd $rootPath/sac/libs/tremor; git checkout *; sh ../convert_tremor_asm.sh; cd - 1>/dev/null
    info "Adding specific toolchain for android..."
    CMAKE_CONFIG=$CMAKE_CONFIG" -DCMAKE_TOOLCHAIN_FILE=$rootPath/sac/build/cmake/toolchains/android.toolchain.cmake\
    -DANDROID_FORCE_ARM_BUILD=ON -DUSE_GRADLE=$USE_GRADLE"
}

compilation_after() {
    info "Cleaning tremor directory..."
    cd $rootPath/sac/libs/tremor; git checkout *; cd - 1>/dev/null

    if [ $USE_GRADLE = 1 ]; then
        if [ ! -f $rootPath/libs/armeabi-v7a.jar ]; then
            error_and_quit "Missing libs/armeabi.jar (did you forgot to compile ?)!"
        fi
        info "TODO for gradle" $red
    else
        rm $rootPath/libs/armeabi-v7a.jar 2>/dev/null
    fi

    if [ $REGENERATE_ANT_FILES = 1 ]; then
        info "Updating android project"
        cd $rootPath
        if ! android update project -p . -t 1 -n $gameName --subprojects; then
            error_and_quit "Error while updating project"
        fi

        if ! ant debug; then
            error_and_quit "Ant failed - see above for the reason"
        fi
    fi
}

launch_the_application() {
    cd $rootPath
    if [ $INSTALL_ON_DEVICE = 1 ]; then
        if [ $USE_GRADLE = 1 ]; then
            info "TODO for gradle" $red
        else
            if [ -f "$rootPath/bin/$gameName.apk" ]; then
                APK=$rootPath/bin/$gameName.apk
                info "Installing on device release APK ($(echo $APK | sed 's|.*/||'))..."
            elif [ -f "$rootPath/bin/${gameName}-debug.apk" ]; then
                APK=$rootPath/bin/${gameName}-debug.apk
                info "Installing on device debug APK ($(echo $APK | sed 's|.*/||'))..."
            else
                APK=$(find $rootPath/bin -name "$gameName*.apk" | head -n1 )
                info "Couldn't find a usual name for the apk... will install '$(echo $APK | sed 's|.*/||')'" $orange
            fi
            adb install -r $APK
        fi
    fi

    if [ ! -z $(echo $1 | grep r) ]; then
        info "Running app '$gameName'..."

        gameNameLower=$(echo $gameName | sed 's/^./\l&/')

        adb shell am start -n net.damsy.soupeaucaillou.$gameNameLower/.${gameName}Activity
    fi

    if [ $RUN_LOGCAT = 1 ]; then
        info "Launching adb logcat..."
        adb logcat -C | grep sac
    fi

    if [ $STACK_TRACE = 1 ]; then
        info "Printing latest dump crashes"
        check_package_in_PATH "ndk-stack" "android-ndk"
        adb logcat | ndk-stack -sym $rootPath/obj/local/armeabi-v7a
    fi
}
