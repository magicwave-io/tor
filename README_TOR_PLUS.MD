Release timeline:
         https://trac.torproject.org/projects/tor/wiki/org/teams/NetworkTeam/CoreTorReleases
		 
Tor Plus hacking on Ubuntu 20.04:
 = checkout the tor plus server lib repo before building this repo
 - install next deps: `apt install pkg-config libevent-dev libssl-dev zlib1g-dev liblzma-dev libzstd-dev automake libjson-c-dev libcurl4-openssl-dev`
 - run the autogen.sh command: `./autogen.sh`
 - run the configure command: `./configure --disable-asciidoc`
 - run the make: `make -j`
 
 # Debug mode on Windows
 Use next Windows environment variable in order to turn on Console for Tor Plus.
 set TOR_PLUS_CMD=1
