FROM debian:10 AS cache
WORKDIR /app
COPY . .
RUN rm -rf docker

FROM debian:10
RUN echo deb http://ftp.debian.org/debian buster-backports main >> /etc/apt/sources.list.d/backports.list
RUN echo deb-src http://ftp.debian.org/debian buster-backports main >> /etc/apt/sources.list.d/backports.list

# Installing Library & Package
RUN apt-get update && \
apt-get install -y expect torsocks \
    links \
    dos2unix git make cmake build-essential \
    libevent-dev libssl-dev zlib1g-dev autotools-dev \
    libcurl4 libcurl4-gnutls-dev libjson-c-dev automake && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /opt/paidpiper/tor_plus

# Building and installing
COPY --from=cache /app .
#COPY . .
RUN cd src/lib/rest && rm -rf build && mkdir -p build && cd build && cmake .. && make
RUN sh autogen.sh && \
    autoreconf -f -i && \
    ./configure  --disable-asciidoc && \
    make && \
    make install
