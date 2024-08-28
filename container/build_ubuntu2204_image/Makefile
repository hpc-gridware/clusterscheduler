#___INFO__MARK_BEGIN_NEW__
###########################################################################
#
#  Copyright 2024 HPC-Gridware GmbH
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
###########################################################################
#___INFO__MARK_END_NEW__

REPOSITORY=hpcgridware
IMAGE_NAME_BUILD=clusterscheduler-builder-ubuntu2204
IMAGE_NAME=clusterscheduler-latest-ubuntu2204

.PHONY: build-dependency-image
build-dependency-image:
	@echo "Building Ubuntu 22.04 image with build dependencies..."
	$(eval BUILD_TIME:=$(shell date -u '+%Y-%m-%d'))
	docker build --no-cache  -t $(REPOSITORY)/$(IMAGE_NAME_BUILD):$(BUILD_TIME) -t $(REPOSITORY)/$(IMAGE_NAME_BUILD):latest -f Dockerfile.buildenv .

.PHONY: push-dependency-image
push-dependency-image:
	@echo "Pushing Ubuntu 22.04 image with build dependencies..."
	if [ -z "$(BUILD_TIME)" ]; then \
		@echo "Please set BUILD_TIME to the image tag you want to push"; \
		exit 1; \
	fi
	@echo "Using $(BUILD_TIME) as latest"
	docker push $(REPOSITORY)/$(IMAGE_NAME_BUILD):$(BUILD_TIME)
	docker push $(REPOSITORY)/$(IMAGE_NAME_BUILD):latest

.PHONY: build-lastest-version-image
build-lastest-version-image:
	@echo "Building image with latest Open Cluster Scheduler..."
	if [ -z "$(BUILD_TIME)" ]; then \
		€echo "Please set BUILD_TIME to the image tag of the Ubuntu 22.04 dep. image"; \
		exit 1; \
	fi
	$(eval BUILD_COMMIT:=$(shell git rev-parse --short HEAD))
	@echo "Using $(BUILD_COMMIT) as tag for the image"
	docker build --no-cache \
		--build-context hpcgridware/clusterscheduler-builder-ubuntu2204=docker-image://hpcgridware/clusterscheduler-builder-ubuntu2204:$(BUILD_TIME)  \
		-t $(REPOSITORY)/$(IMAGE_NAME):$(BUILD_COMMIT) \
		-t $(REPOSITORY)/$(IMAGE_NAME):latest \
		--build-arg BUILD_COMMIT=$(BUILD_COMMIT) \
		--build-arg BUILD_TIME=$(BUILD_TIME) \
        -f Dockerfile.builder .

.PHONY: push-lastest-version-image
push-lastest-version-image:
	@echo "Pushing image with latest unstable Open Cluster Scheduler packages..."
	$(eval BUILD_COMMIT:=$(shell git rev-parse --short HEAD))
	docker push $(REPOSITORY)/$(IMAGE_NAME):$(BUILD_COMMIT)
	docker push $(REPOSITORY)/$(IMAGE_NAME):latest

.PHONY: clean
clean:
	@echo "Removing Docker image..."
	docker rmi $(REPOSITORY)/$(IMAGE_NAME_BUILD)
