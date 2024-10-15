# TREX: A Toolbox for Reverse Engineering

This repository contains the source code of TREX, a toolbox containing reusable software analysis tools to mimick real-world reverse engineering attacks.

## Use cases

The toolbox is described in the paper *Thomas Faingnaert, Tab Zhang, Willem Van Iseghem, Gertjan Everaert, Bart Coppens, Christian Collberg, and Bjorn De Sutter. 2024. Tools and Models for Software Reverse Engineering Research. In Proceedings of the 2024 Workshop on Research on offensive and defensive techniques in the context of Man At The End (MATE) attacks (CheckMATE ’24), October 14–18, 2024, Salt Lake City, UT, USA.. ACM, New York, NY, USA, 15 pages. https://doi.org/10.1145/3689934.3690817*.
It contains three use cases: cryptographic key localisation, license key localisation, and game resource hacking.

### Cryptographic key localisation

This use case localises cryptographic keys and encryption/decryption algorithms in binaries.
It is a reimplementation and extension of the state-of-the-art technique K-Hunt.
This use case is described in the paper *Thomas Faingnaert, Willem Van Iseghem, and Bjorn De Sutter. 2024. K- Hunt++: Improved Dynamic Cryptographic Key Extraction. In Proceedings of the 2024 Workshop on Research on offensive and defensive techniques in the context of Man At The End (MATE) attacks (CheckMATE ’24), October 14–18, 2024, Salt Lake City, UT, USA.. ACM, New York, NY, USA, 8 pages. https://doi.org/10.1145/3689934.3690818*.

This use case is implemented in the Jupyter notebook `notebooks/evaluate-localisation.ipynb` and its corresponding Python files in `notebooks/evaluate_localisation/`.
To run this use case, follow the instructions in [Set-up instructions](#set-up-instructions), and the [Notebooks](#notebooks) section for instructions on how to run the Jupyter notebooks.
More information on this specific use case is embedded in the [notebook file itself](notebooks/evaluate-localisation.ipynb).

### License key localisation

This use case focuses on localisating a simple license key check.
The use case is implemented in the demo `demos/license-key/demo.py`.
To run this use case, first perform the initial setup by following [Set-up instructions](#set-up-instructions), and then the [Demos](#demos) section.

### Game resource hacking

This use case largely automates the localisation of instructions modifying a certain resource in a game.
This use case is implemented in the demo `demos/supertux/demo.py`.
To run this use case, first perform the initial setup by following [Set-up instructions](#set-up-instructions), and then the [Demos](#demos) section.
Further, you will also need to follow the instructions outlined in [the demo's README](demos/supertux/README.md#set-up-instructions).

## Set-up instructions

### Base requirements

First, install the following packages using your package manager:
- binutils
- Docker. Also make sure that you can access the Docker socket as your currently logged in user, e.g. by adding yourself to the `docker` group.
- Python 3.6 or newer (don't forget to install the -dev version as well!)
- virtualenv (optional)
- libffi-dev (for cffi), libjpeg-dev (for pillow)

To (optionally) create a virtual environment for development, run:
```bash
cd TREX/
virtualenv virtualenv
source virtualenv/bin/activate
```

Then install the framework's dependencies with `pip install -r requirements.txt`.

### Integration with external tools

TREX integrates with a variety of external reverse engineering tools.
If you want to use these, you will need to perform some additional steps for each tool.

#### LLDB

To use LLDB, you need to install LLDB and its Python bindings via your package manager, e.g. using `sudo apt install -y lldb-11`.
This is because LLDB's Python interface is shipped with LLDB itself, and thus cannot be installed in a virtual environment via pip.
You also need to store the path to LLDB's Python files (e.g. `/usr/lib/python3/dist-packages`) in the `LLDB_PYTHON_MODULE_PATH` environment variable.
You can find this path by running the following in a _system_ Python REPL:
```python
import os
import lldb
os.path.abspath(os.path.join(os.path.dirname(lldb.__file__), '../'))
```

#### Binary Ninja

For integration with Binary Ninja, see [the respective README](integration/binaryninja/README.md).

#### IDA Pro

If use of IDA is desired, place the installer into the correct folder (refer to
[IDA's pre-installation section](containers/ida/README.md#pre-running-installation-requirements) for more information).

#### Intel SDE

If you want to use SDE (required for all use cases), you will need to download Intel SDE 9.38 from Intel's website (https://www.intel.com/content/www/us/en/download/684897/823664/intel-software-development-emulator.html), and place the resulting `.tar.xz` file at `containers/pin/container/sde-external-9.38.0-2024-04-18-lin.tar.xz`.

## Usage instructions

There are two ways to instantiate attacks in TREX: either using a `demo.py` script, or using a Jupyter notebook if more interactivity is required.

### Demos

You can find several examples of demos in `demos/*/demo.py`.
In order to run a demo, pass its path to the `main.py` script as follows:

```python
python3 main.py demos/memorycopy/demo.py
```

### Notebooks

The `notebooks/` subdirectory contains Jupyter notebooks containing different use cases of the framework.
You can open a notebook by running `jupyter-lab` in your Python virtual environment, and opening the corresponding file in Jupyter Lab in your browser.

## Instructions for Developers

### Project layout

```
├── containers/                 Dockerfiles for the modules and plugins.
│   ├── <tool>/                 Dockerfile(s) and configuration for <tool>.
│   └── pin/                    Dockerfile for Intel SDE (Pin), and Pintools.
│       └── sources/            The source code of the Pintools.
├── core/                       Framework kernel classes providing intialisation, access to the database, and workspaces.
├── data/                       Used by the database container for storage.
├── demos/                      Contains the example use case applications.
├── graph_models/               OGM classes for defining some of the analysis structures in the DB.
├── import/                     Used by the DB container for I/O.
├── integration/                Plugins for integration with external software.
├── KNOWN-ISSUES.md             File containing known issues.
├── main.py                     Main entry point to run demos (cfr. supra).
├── modules/                    Python classes that implement a particular software analysis.
├── notebooks/                  Jupyter notebooks with examples of different use cases of the framework.
├── plugins/                    Python classes providing reusable functionality for analyses.
├── query_language/             Python implementation of the query language.
└── workspaces/                 Folder for analysis I/O and where temporary files are generated, seperated by project.
```

### LICENSE

TREX is available under the licensing terms specified in `LICENSE`.
The code in the `demos/` subdirectory is licensed under the terms specified in the respective subdirectory.
