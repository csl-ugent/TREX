#!/usr/bin/env python3

import sys
import os
import math

from pygments import highlight
from pygments.lexers import NasmLexer, CppLexer
from pygments.formatters import TerminalTrueColorFormatter

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.append(os.path.dirname(SCRIPT_DIR))

from core.core import Core
from core.workspace import Workspace

from query_language.expressions import *
from query_language.node_matchers import *
from query_language.traversal_matchers import *

from query_language.get_query import get_query

workspace = input('Workspace? ')

Workspace.load_from_db(Core())
Workspace.select(workspace)
db = Workspace.current.graph

source_base_path = input('Source directory base path: ')

# Prompt for starting instruction.
starting_image_name = input('Image name? ')
starting_image_offset = int(input('Image offset? '), 16)

path = [(starting_image_name, starting_image_offset)]


def normalise(s):
    while s.startswith('../'):
        s = s.removeprefix('../')

    return s


def hi(s, lang):
    if lang == 'asm':
        return highlight(s, NasmLexer(), TerminalTrueColorFormatter(style='colorful')).strip()
    if lang == 'cpp':
        return highlight(s, CppLexer(), TerminalTrueColorFormatter(style='colorful')).strip()


def pretty_print_ins(ins, print_source=False, prefix=''):
    prefix = '' if prefix == '' else f'{prefix} '

    image_name, image_offset = ins

    rv = db.run_evaluate(get_query(
        Instruction(
            P('image_name') == image_name,
            P('image_offset') == image_offset
        ).bind('ins')
    ))

    friendly_location = f"{image_name}+0x{hex(image_offset)}"
    asm = f"{rv['opcode'] or '???':15} {rv['operands'] or '???'}"
    print(f"{prefix}{friendly_location:20} {rv['routine_name'] or '???':30} {hi(asm, 'asm')}")

    source_path = os.path.join(source_base_path, normalise(rv['DEBUG_filename'] or '???'))

    if print_source:
        if os.path.exists(source_path):
            print()

            with open(source_path) as f:
                last_function = '(unknown function)'

                for i, line in enumerate(f.readlines()):
                    if not line.startswith(' ') and line.strip() != '{' and line.strip() != '':
                        last_function = line.strip()

                    if abs(i+1-rv['DEBUG_line']) <= 3:
                        if i+1 == rv['DEBUG_line'] - 3:
                            print(f'╭ {hi(last_function, "cpp")}')

                        if i+1 == rv['DEBUG_line']:
                            print('│')

                        print(f'│ {i:4} {hi(line, "cpp")}')

                        if i+1 == rv['DEBUG_line']:
                            print('│')

                        if i+1 == rv['DEBUG_line'] + 3:
                            print('╰')
        else:
            print(f'File {source_path} not found.')

last_action = ''

while True:
    os.system('clear')

    for ins in path:
        pretty_print_ins(ins, print_source=True)

    print()

    rv = db.run_list(get_query(
        Instruction(
            P('image_name') == path[-1][0],
            P('image_offset') == path[-1][1],

            dependsOn(
                Instruction().bind('ins')
            ).bind('depends')
        )))

    rv = list(dict((f"{res['ins']['image_name']}__{res['ins']['image_offset']}__{res['depends']['register']}__{res['depends']['address'] is None}", res) for res in rv).values())
    rv = sorted(rv, key=lambda x: x['ins']['image_offset'])

    print('Dependencies:')
    for i, res in enumerate(rv):
        reg = res['depends']['register']
        mem = res['depends']['address']

        prefix = '(mem)' if mem is not None else reg
        prefix = f'{i:2} {prefix:5}'

        pretty_print_ins((res['ins']['image_name'], res['ins']['image_offset']), prefix=prefix)

    print()
    action = input('[number]|[u]p|[q]uit> ')

    last_action = action = action if action != '' else last_action

    if action == 'q':
        exit(0)
    elif action == 'u':
        if len(path) > 1:
            path.pop()
    else:
        n = int(action)

        path.append((rv[n]['ins']['image_name'], rv[n]['ins']['image_offset']))
