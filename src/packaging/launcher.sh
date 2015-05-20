#!/system/bin/sh
root=$(cd $(dirname $0) && pwd)
app=$(basename $0)
realapp=${root}/bin/${app}
echo "root=${root} -- app=${app}"
(
    cd ${root}/bin
    LD_LIBRARY_PATH=${root}/lib:${LD_LIBRARY_PATH} ${realapp} "$@"
)