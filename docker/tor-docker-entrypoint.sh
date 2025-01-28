#!/bin/bash
set -e
# check role
case "$role" in
 client) echo CLIENT ;;
 dirauth) echo DIRAUTH ;;
 exit) echo EXIT ;;
 hs_client) echo HS_CLIENT ;;
 *) sleep 1 && echo "role value is not valid $role" && exit 1 ;;
esac

case "$PP_ENV" in
 stage) echo STAGE ;;
 prod) echo PROD ;;
 *) sleep 1 && echo "PP_ENV not valid $role" && exit 1 ;;
esac
#export data_directory="/Users/tumarsal/tor"

if [[ "${no_conf}" != "1" ]]; then
  source /opt/paidpiper/tor.${PP_ENV}.cfg
  export dirauth=$dirauth
  export data_directory="/root/tor"

  randNickname="t$(cat /proc/sys/kernel/random/uuid | sed 's/-//g')"
  export nickname="${randNickname:0:10}"

  export inventory_hostname=$nickname
  if [[ -z "${self_host}" ]]; then
   export self_host="$(ip addr | grep 'state UP' -A2 | tail -n1 | awk '{print $2}' | cut -f1  -d'/')"
  fi

  mkdir -p /usr/local/etc/tor/ && cat /opt/paidpiper/configs/${role}_torrc.tmpl | envsubst > /usr/local/etc/tor/torrc
fi
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
fn_exists() { declare -F "$1" > /dev/null; }
function checkTorReady {
    case "$role" in
    client) mark "Bootstrapped 100% (done): Done" "/opt/paidpiper/.tor_ready" ;; # todo check
    dirauth) mark "Published microdesc consensus" "/opt/paidpiper/.tor_ready" ;;
    exit) mark "Performing bandwidth self-test...done" "/opt/paidpiper/.tor_ready" ;;
    hs_client) mark "Bootstrapped 100% (done): Done" "/opt/paidpiper/.tor_ready" ;;
    esac
}

if [ ! -f /opt/paidpiper/.pg_ready ]; then
  fn_exists pg_start && pg_start
fi

while [ ! -f /opt/paidpiper/.pg_ready ]; do
  sleep 2 # or less like 0.2
  echo "pg not ready yet..."
done

if [[ "${role}" = "hs_client" ]]; then
  mkdir -p /root/tor/hidden_service/hsv3
  chmod u=rwx,g=-,o=- /root/tor/hidden_service/hsv3
fi

if [ $# -eq 0 ]
then
    /usr/local/bin/tor -f /usr/local/etc/tor/torrc | checkTorReady
else
    exec "$@" | checkTorReady
fi

