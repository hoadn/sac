#how to use the script
export PLATFORM_OPTIONS="\
-p|-push: call push-build-on-website script to push on a distant server a build"

parse_arguments() {
    ret=0
    should_push=""
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

            "-p" | "-push")
                # cheating - but we don't want targets to be empty (else it will display the help)
                targets=$targets" "
                should_push="y"
                ;;

            --*)
                ret=$(($ret + 1))
                info "Unknown option '$1', ignoring it and its arg '$2'" $red
                shift
                ;;
            -*)
                ret=$(($ret + 1))
                info "Unknown option '$1', ignoring it" $red
                ;;
        esac
        shift
    done
    return $ret
}

check_necessary() {
    check_package nodejs
}

init() {
    :
}

compilation_before() {
    :
}

compilation_after() {
    :
}

launch_the_application() {
    #push on site required
    if [ ! -z "$should_push" ]; then
        info "Pushing content on site..."
        ../../sac/tools/build/push-build-on-website.sh -d . -i
    fi

    #launch required
    if [ ! -z "$(echo $targets | grep r)" ]; then
        info "Finding a navigator..."
        #find a navigator...
        navigator=""
        if ( type iceweasel 1>2 &2>/dev/null ); then
            info "iceweasel will be used"
            navigator="iceweasel"
        elif (type chromium 1>2 &2>/dev/null ) ;then
            info "iceweasel will be used"
            navigator="iceweasel"
        else
            info "can't find any navigator to view the result!" $red
        fi

        if [ ! -z $navigator ]; then
            info "Launch game in ${navigator}."
            $navigator $(pwd)/$gameName.html
        fi
    fi
}


















