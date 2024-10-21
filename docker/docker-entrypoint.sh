#!/bin/bash
set -e
# check role
case "$role" in
 client) echo CLIENT ;;
 dirauth) echo DIRAUTH ;;
 exit) echo EXIT ;;
 hs_client) echo CLIENT ;;
 *) sleep 1 && echo "role value is not valid $role" && exit 1 ;;
esac

function mark {
  match=$1
  file=$2
  mark=1
  while read -r data; do
    echo $data
    if [[ $data == *"$match"* ]]; then 
      if [[ "$mark" == "1" ]]; then 
        echo "done" >> $file
        mark=0
      fi
    fi
  done
}

function checkTorReady {
    case "$role" in
    client) echo CLIENT ;;
    dirauth) mark "Published microdesc consensus" ".done" ;;
    exit) mark "Performing bandwidth self-test...done" ".done" ;;
    hs_client) echo CLIENT ;;
    esac
}
cat > fastlane/Fastfile << EOL

EOL

#sh echoer.sh | tee >(grep "ehco2" | head -1 | touch .done) | tee ./log.log 
#sh echoer.sh | tee >(head -1 1>&2) | tee ./log.log 
#sh echoer.sh | checkTorReady | tee ./log.log 
exec "$@" | checkTorReady
