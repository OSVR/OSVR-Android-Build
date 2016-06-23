#!/system/bin/sh -x
root=$(cd $(dirname $0) && pwd)
launcher=${root}/bin/launcher.sh
TOOLBOX=$(which toolbox)
if which chmod > /dev/null; then
    CHMOD=$(which chmod)
else
    CHMOD="${TOOLBOX} chmod"
fi


if which ln > /dev/null; then
    LN=$(which ln)
else
    LN="${TOOLBOX} ln"
fi

make_executable() {
    $CHMOD 775 $@
}

make_executable ${launcher}

for app in osvr_server osvr_print_tree AnalogCallback_cpp ButtonCallback_cpp DisplayParameter_cpp PathTreeExport TrackerCallback_cpp; do
    if [ -e ${root}/bin/${app} ]; then
        make_executable ${root}/bin/${app}
        rm -f ${root}/${app}
        $LN -s ${launcher} ${root}/${app}
        make_executable ${root}/${app}
    else
        rm -f ${root}/${app}
    fi
done
