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
 *) sleep 1 && echo "PP_ENV not valid $PP_ENV" && exit 1 ;;
esac

if [[ "${PP_SINGLEHOP_HS}" != "" ]]; then
  case "$PP_SINGLEHOP_HS" in
  0) echo SINGLEHOP HS OFF ;;
  1) echo SINGLEHOP HS ON;;
  *) sleep 1 && echo "PP_SINGLEHOP_HS not valid $PP_SINGLEHOP_HS" && exit 1 ;;
  esac
fi

#export data_directory="/Users/tumarsal/tor"
if [[ "${INITIALIZE_TOR}" == "true" ]]; then
  source /opt/${BASE_NAME}/tor.${PP_ENV}.cfg
  export dirauth=$dirauth
  export data_directory="/root/tor"
  export hs_directory="/root/hidden_service"
  if [[ "${nickname}" == "" ]]; then
    sleep 1 && echo "Nickname not setted" && exit 1;
  fi

  export inventory_hostname=$nickname
  if [[ -z "${self_host}" ]]; then
   export self_host="$(ip addr | grep 'state UP' -A2 | tail -n1 | awk '{print $2}' | cut -f1  -d'/')"
  fi
  mkdir -p /usr/local/etc/tor/ && cat /opt/${BASE_NAME}/configs/${role}_torrc.tmpl | envsubst > /usr/local/etc/tor/torrc
  if [[ "${PP_SINGLEHOP_HS}" = "1" ]]; then
    echo -e "\nHiddenServiceSingleHopMode 1\nHiddenServiceNonAnonymousMode 1\nHiddenServiceNonAnonymousModeClient 1\n" >> /usr/local/etc/tor/torrc
  fi
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
    client) mark "Bootstrapped 100% (done): Done" "/opt/${BASE_NAME}/.tor_ready" ;; # todo check
    dirauth) mark "Published microdesc consensus" "/opt/${BASE_NAME}/.tor_ready" ;;
    exit) mark "Performing bandwidth self-test...done" "/opt/${BASE_NAME}/.tor_ready" ;;
    hs_client) mark "Bootstrapped 100% (done): Done" "/opt/${BASE_NAME}/.tor_ready" ;;
    esac
}

if [ ! -f /opt/${BASE_NAME}/.pg_ready ]; then
  fn_exists pg_start && pg_start
fi

while [ ! -f /opt/${BASE_NAME}/.pg_ready ]; do
  sleep 2 # or less like 0.2
  echo "pg not ready yet..."
done
chmod u=rwx,g=-,o=- /root/tor
if [[ "${role}" = "hs_client" ]]; then
  mkdir -p /root/hidden_service/hsv3
  chmod u=rwx,g=-,o=- /root/hidden_service/hsv3
fi

if [ $# -eq 0 ]
then
     /usr/local/bin/tor -f /usr/local/etc/tor/torrc | checkTorReady &> /opt/${BASE_NAME}/logs/tor.log
else
    exec "$@" | checkTorReady
fi

