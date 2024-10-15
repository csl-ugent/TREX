# IDA pro

More information on IDA pro: https://www.hex-rays.com/products/ida/

## Version

Current used version is IDA 7.5.

**Note that if you want to deviate from this version, you might have to update the scripts in the `plugin` folder as well
to adjust to API changes.**

## Pre-running installation requirements

Before building, complete the next steps:
 - place your IDA installer in the supplied folder (`container\installer`) and make sure it's named `ida_installer.run`.
 - place the installation password in the same folder and name it `ida.installpwd`.

## Manually building the image

In case of problems you can manually build the image with the command below to troubleshoot the build process:

```
cd container
docker build . --build-arg IDA_PASSWORD="your-pw-here" -t mate_attack_framework_ida
```

## Manually running the image

If you need to run the image manually, make sure you pass the necessary environment variables and volumes so that
X-server support can be passed through.

An example of running the container with environment variables and volumes is as follows (should work on most distros):

`docker run -it -e DISPLAY=$DISPLAY -v ${HOME}/.Xauthority:/root/.Xauthority -v /tmp/.X11-unix:/tmp/.X11-unix -v $(pwd)/target:/target -v $(pwd)/plugins:/plugins --privileged  mate_attack_framework_ida`

You may need to `xhost +local:docker` to allow Docker to access Xorg.
