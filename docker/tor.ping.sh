
PP_ENV="stage"
set -x #echo on
if [[ "${PP_ENV}" = "stage" ]]; then
    hostNamePath=/var/lib/tor/hidden_service_v3/hostname
    bootstrapServer="13.95.67.71"

fi
if [[ "${PP_ENV}" = "prod" ]]; then
    hostNamePath=/var/lib/tor/hidden_service_paidpiper/hostname
    bootstrapServer="40.68.195.206"

fi
fullOnionAddress=$(ssh edikk202@${bootstrapServer} "cat ${hostNamePath}")

curl -I -x socks5h://127.0.0.1:9050 ${fullOnionAddress}:8008
curl -I -x socks5h://127.0.0.1:9050 ${fullOnionAddress}:8088
curl -I -x socks5h://127.0.0.1:9050 ${fullOnionAddress}:8080
curl -I -x socks5h://127.0.0.1:9050 ${fullOnionAddress}:4001
curl -I -x socks5h://127.0.0.1:9050 ${fullOnionAddress}:5001
#curl -v --socks5-hostname localhost:9050 ${fullOnionAddress}:8008/zaets.mp4 -o /dev/null
