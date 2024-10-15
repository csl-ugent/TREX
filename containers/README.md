# Containers

This folder contains the Docker containers that the framework is using for certain functionality.

## Installation

Normally the Docker containers should be pulled in automatically or built on-the-fly when needed. If this would be 
failing for any reason, you can always build them manually to diagnose the issue.

For building a container manually, please refer to the README inside the respective containers for more information.

## Adding new containers

To add a new container, it's easiest to follow this procedure:

- Add a new folder that's aptly named
- Add a new python file that contains the runner. This runner should inherit from the `ContainerRunner` class, which
  implements most functionality that's needed already. Most used options for running a Docker container are present as
  option in the init method, but if needed you can add a keyword argument that's directly passed to the Docker API
  method. Refer to [the API documentation of Docker-py](https://docker-py.readthedocs.io/en/stable/containers.html#docker.models.containers.ContainerCollection.run)
  for more information.
- If a container image needs to be built, indicate this in the class above, and create a `Dockerfile` in the `container`
  folder that you created in the first step. 