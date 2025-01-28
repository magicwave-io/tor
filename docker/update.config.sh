
file="tor.${PP_ENV}.cfg"
if [[ "${PP_ENV}" = "stage" ]]; then
    bootstrapServer="13.95.67.71"
fi

if [[ "${PP_ENV}" = "prod" ]]; then
    bootstrapServer="40.68.195.206"
fi
ssh edikk202@${bootstrapServer} "touch ~/.hushlogin"

echo "${PP_ENV}"
echo "dirauth=\"" > tor.${PP_ENV}.cfg
ssh edikk202@${bootstrapServer} "cat /usr/local/etc/tor/torrc | grep DirAuthority" >> "${file}"
echo "\"" >> tor.${PP_ENV}.cfg
