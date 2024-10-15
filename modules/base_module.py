from core.workspace import Workspace


class ConvenienceModule:
    def __init__(self):
        # Set some convenience properties
        self.binary_path: str = Workspace.current.binary_path
        self.binary_name: str = Workspace.current.binary_name
        self.binary_is64bit: bool = Workspace.current.binary_is64bit
        self.tmp_path: str = Workspace.current.tmp_path
        self.workspace_path: str = Workspace.current.path


class BaseModule(ConvenienceModule):
    def __init__(self, input_analysis=None):
        super().__init__()
        if input_analysis and hasattr(self, 'inputTemplates'):
            # Verify input analysis type
            if input_analysis.__class__ not in self.inputTemplates:
                class_names = [klass.__name__ for klass in self.inputTemplates]
                raise TypeError("{} requires input analysis of type {}".format(self.__class__.__name__, class_names))

        # Create analysis node
        self.analysis = self.outputTemplate()
        self.analysis.module = self.__class__.__name__
        if input_analysis:
            self.analysis.ancestors.add(input_analysis)
