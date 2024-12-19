
# Build binaries
VERSION:=$(shell git describe --tags --dirty --always)
COMMIT_HASH:=$(shell git rev-parse --short HEAD 2>/dev/null)
BUILD_DATE:=$(shell date "+%Y-%m-%d")

build_image:
	echo "/docker" > ../.dockerignore
	docker build -t tor_build -f Build.Dockerfile ../
	rm -rf ../.dockerignore
image:
	make image_last
image_last:
	rm -rf ../.dockerignore
	docker build --build-arg TOR_VERSION=$(VERSION) \
                --build-arg PP_ENV=$(PP_ENV) \
                --build-arg TOR_BUILD_HASH=$(COMMIT_HASH) \
                --build-arg TOR_BUILD_DATE=$(BUILD_DATE) \
                -t torplus \
                -t torplusserviceregistry.azurecr.io/private/tor:$(VERSION) \
                -t torplusserviceregistry.azurecr.io/private/tor:latest \
                -f Dockerfile ../

az_pipeline_image:
	rm -rf ../.dockerignore
	docker build --build-arg TOR_VERSION=$(VERSION) \
		--build-arg PP_ENV=$(PP_ENV) \
		-t torplus \
		-t torplusserviceregistry.azurecr.io/private/tor:$(VERSION) \
		-t torplusserviceregistry.azurecr.io/private/tor:latest \
		-f Dockerfile_az_pipeline ../
push:
	docker push torplusserviceregistry.azurecr.io/private/tor:$(VERSION)
	docker push torplusserviceregistry.azurecr.io/private/tor:latest
az_pipeline_push: az_pipeline_image
	docker push torplusserviceregistry.azurecr.io/private/tor:$(VERSION)
	docker push torplusserviceregistry.azurecr.io/private/tor:latest
clean_up:
	docker rmi --force \
	torplus \
	torplusserviceregistry.azurecr.io/private/tor:$(VERSION) \
	torplusserviceregistry.azurecr.io/private/tor:latest
	docker image prune --force
run_it:
	docker run -it --name torplus --rm --entrypoint /bin/bash torplusserviceregistry.azurecr.io/private/tor

attach:
	docker exec -it torplus bash

run_it_builder:
	docker run -it --name torplus_build --rm --entrypoint /bin/bash torplus_build

update_config:
	PP_ENV=stage sh update.config.sh
update_config_prod:
	PP_ENV=prod sh update.config.sh
ping:
	sh tor.ping.sh
run:
	sh tor.start.sh
