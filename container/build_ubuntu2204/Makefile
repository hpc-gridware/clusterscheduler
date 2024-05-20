IMAGE_NAME=clusterscheduler-image-ubuntu2204
CONTAINER_NAME=clusterscheduler-ubuntu2204
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
