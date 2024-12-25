export PP_ENV=prod
export role=hs_client
sh update.config.sh
source tor.${PP_ENV}.cfg
export dirauth=$dirauth
export data_directory="$HOME/tor"
export nickname="tumar123"
export inventory_hostname=$nickname

if [[ "${role}" = "hs_client" ]]; then
  mkdir $HOME/tor
  mkdir -p $HOME/tor/hidden_service/hsv3
  chmod -R u=rwx,g=-,o=- $HOME/tor/
  chmod u=rwx,g=-,o=- $HOME/tor/hidden_service/hsv3
fi
cat configs/${role}_torrc.tmpl | envsubst > /usr/local/etc/tor/torrc
echo "SHOW CONFIG: cat /usr/local/etc/tor/torrc"
if [[ "${PP_ENV}" = "prod" ]]; then
  ./tor_plus -f /usr/local/etc/tor/torrc
else
   tor -f /usr/local/etc/tor/torrc
fi


