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

DOCKER_USER=hpcgridware
IMAGE_VERSION=1.0
IMAGE_NAME=clusterscheduler-image-rocky89
CONTAINER_NAME=clusterscheduler-rocky89
MOUNT_DIR=$(shell pwd)/../../..
BUILD_AND_TEST=/clusterscheduler/clusterscheduler/container/build_and_test.sh

.PHONY: build
build:
	@echo "Building Docker image..."
	docker build -t $(DOCKER_USER)/$(IMAGE_NAME) .
	docker tag $(DOCKER_USER)/$(IMAGE_NAME) $(DOCKER_USER)/$(IMAGE_NAME):$(IMAGE_VERSION)
	docker tag $(DOCKER_USER)/$(IMAGE_NAME) $(DOCKER_USER)/$(IMAGE_NAME):latest

.PHONY: push
push:
	@echo "Pushing Docker image..."
	@echo "make sure to have done docker login"
	docker push -a $(DOCKER_USER)/$(IMAGE_NAME)

.PHONY: interactive
interactive:
	@echo "Running Docker container with parent directory mounted..."
	@echo "Build OCS: cmake -S /clusterscheduler/clusterscheduler"
	@echo "Build GCS: cmake -S /clusterscheduler/clusterscheduler -DPROJECT_EXTENSIONS=/clusterscheduler/gcs-extensions"
	@echo "           -DPROJECT_FEATURES=gcs-extensions"
	docker run -it --rm --name $(CONTAINER_NAME) -v $(MOUNT_DIR):/clusterscheduler $(DOCKER_USER)/$(IMAGE_NAME):latest

.PHONY: ocs
ocs:
	docker run --rm --name $(CONTAINER_NAME) -v $(MOUNT_DIR):/clusterscheduler $(DOCKER_USER)/$(IMAGE_NAME) $(BUILD_AND_TEST) -bashrc -doc

.PHONY: gcs
gcs:
	docker run --rm --name $(CONTAINER_NAME) -v $(MOUNT_DIR):/clusterscheduler $(DOCKER_USER)/$(IMAGE_NAME) $(BUILD_AND_TEST) -bashrc -gcs -doc

.PHONY: clean
clean:
	@echo "Removing Docker image..."
	docker rmi $(DOCKER_USER)/$(IMAGE_NAME)
