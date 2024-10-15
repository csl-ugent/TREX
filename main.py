import IPython
import argparse

import importlib.util

from core.core import Core
from core.workspace import Workspace

if __name__ == '__main__':
    # Get the demo to run.
    parser = argparse.ArgumentParser(description='Run demo in MATE attack framework')
    parser.add_argument('demo', help='Path to the demo to run (e.g. demos/sevenzip/demo.py)')
    parser.add_argument('demo_arguments', help='Arguments for the demo (if applicable)', nargs=argparse.REMAINDER)
    args = parser.parse_args()

    # Import correct demo.
    spec = importlib.util.spec_from_file_location('demo', args.demo)
    demo = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(demo)

    # Load workspaces from database.
    Workspace.load_from_db(Core())

    # Run demo.
    print("\n\n++++++++++++++++ Demo start +++++++++++++++++++")
    demo.demo(*args.demo_arguments)
    print("\n\n++++++++++++++++ Demo finished ++++++++++++++++")
