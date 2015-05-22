#!/system/bin/sh
root=$(cd $(dirname $0) && pwd)
launcher=${root}/bin/launcher.sh

make_executable() {
    busybox chmod +x $@
}

make_executable ${launcher}

for app in osvr_server osvr_print_tree AnalogCallback_cpp ButtonCallback_cpp DisplayParameter_cpp PathTreeExport TrackerCallback_cpp; do
    if [ -e ${root}/bin/${app} ]; then
        make_executable ${root}/bin/${app}
        rm -f ${root}/${app}
        ln -s ${launcher} ${root}/${app}
        make_executable ${root}/${app}
    else
        rm -f ${root}/${app}
    fi
done
