import os

from mate_attack_framework.util import get_function_containing_address

if os.environ.get('MATE_FRAMEWORK_BINARYNINJA_INTEGRATION_HEADLESS') is None:
    from binaryninjaui import DockContextHandler, UIActionHandler

    from PySide6.QtWidgets import QWidget

    class DockBase(QWidget, DockContextHandler):
        '''Utility class that is used as a base to all docks.'''

        def __init__(self, parent, name, ui = None):
            QWidget.__init__(self, parent)
            DockContextHandler.__init__(self, self, name)

            self.actionHandler = UIActionHandler()
            self.actionHandler.setupActionHandler(self)

            self.function = None
            self.offset = 0
            self.binary_view = None

            self.ui = ui

            if self.ui is not None:
                self.ui.setupUi(self)

        def onOffsetChanged(self):
            # Default implementation. Derived classes should override this.
            pass

        def notifyOffsetChanged(self, offset):
            self.offset = offset
            self.function = get_function_containing_address(self.binary_view, self.offset)

            self.onOffsetChanged()

        def shouldBeVisible(self, view_frame):
            return view_frame is not None


        def notifyViewChanged(self, view_frame):
            if view_frame is None:
                self.binary_view = None
                self.function = None
            else:
                self.binary_view = view_frame.getCurrentViewInterface().getData()
                self.function = get_function_containing_address(self.binary_view, self.offset)
