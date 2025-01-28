
file="tor.${PP_ENV}.cfg"
if [[ "${PP_ENV}" = "stage" ]]; then
    bootstrapServer="edikk202@13.95.67.71"
fi

if [[ "${PP_ENV}" = "prod" ]]; then
    bootstrapServer="TorPlus@172.105.244.252"
fi
ssh ${bootstrapServer} "touch ~/.hushlogin"

echo "${PP_ENV}"
echo "dirauth=\"" > tor.${PP_ENV}.cfg
ssh ${bootstrapServer} "cat /usr/local/etc/tor/torrc | grep DirAuthority" >> "${file}"
echo "\"" >> tor.${PP_ENV}.cfg
