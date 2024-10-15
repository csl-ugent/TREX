executed = False
output_basename = ""
smart_attacker = False


def dump(x):
    for attr in dir(x):
        if attr.startswith('__'):
            continue
        print(".%s=%r" % (attr, getattr(x, attr)))


def print_debug(x):
    pass


def SortedDictionaryIterator(dct):
    for key in sorted(dct):
        yield key, dct[key]
