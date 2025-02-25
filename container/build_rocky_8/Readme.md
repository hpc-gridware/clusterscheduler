We use this docker image for building Open Cluster Scheduler via github "on push" action.

The image is based on Rocky 8 and contains all necessary dependencies to build Open Cluster Scheduler.

To update it, you need to build it locally and push it to the docker registry.

```bash
docker login -u <username>
docker rmi hpcgridware/clusterscheduler-image-rocky89:latest
docker build -t hpcgridware/clusterscheduler-image-rocky89:latest .
docker push hpcgridware/clusterscheduler-image-rocky89:latest
```