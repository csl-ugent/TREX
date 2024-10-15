# Known Issues

## Query Language - Circular Imports

Currently there are some issues with circular imports in the *query language* implementation. Presumably, the issue is limited to the `query_language.node_matchers.py` and `query_language.traversal_matchers.py` files.

### Symptoms

When importing a module from the framework you get errors similar to:

`ModuleNotFoundError: No module named 'mate_attack_framework.query_language'`

and

`ImportError: cannot import name 'TraversalBase' from partially initialized module 'query_language.traversal_matchers' (most likely due to a circular import)`

### Workaround

Before importing any other module from the framework add the following lines to your  notebook or python file:

```python
from query_language.expressions import *
from query_language.node_matchers import *
from query_language.traversal_matchers import *
```
