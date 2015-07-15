#!/system/bin/sh
root=$(cd $(dirname $0) && pwd)
launcher=${root}/bin/launcher.sh

if which chmod > /dev/null; then
    $CHMOD=chmod
else
    $CHMOD=toolbox chmod
fi


if which ln > /dev/null; then
    $LN=ln
else
    $LN=toolbox ln
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
