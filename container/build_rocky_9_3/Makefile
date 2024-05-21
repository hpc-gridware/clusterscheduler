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

IMAGE_NAME=clusterscheduler-image-rocky93
CONTAINER_NAME=clusterscheduler-rocky93
MOUNT_DIR=$(shell pwd)/../..

.PHONY: build
build:
	@echo "Building Docker image..."
	docker build -t $(IMAGE_NAME) .

.PHONY: run
run:
	@echo "Running Docker container with parent directory mounted..."
	docker run -it --rm --name $(CONTAINER_NAME) -v $(MOUNT_DIR):/clusterscheduler $(IMAGE_NAME)

.PHONY: clean
clean:
	@echo "Removing Docker image..."
	docker rmi $(IMAGE_NAME)
