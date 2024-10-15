EDGE_FALLTHROUGH = 0
EDGE_JUMP = 1
EDGE_CALL = 2
EDGE_CALLFALLTHROUGH = 3

INVALID_BBL = -1


class Tool(object):
    def __init__(self, name, vanilla):
        self.__name = name
        self._instructions = {}
        self._bbls = {}
        self._functions = {}
        self._edges = {}
        self._vanilla = vanilla
        self._transformations = {
            'transformation_data': {},
            'transformation_by_phase': {}
        }

    def TransformationIsObfucation(self, tf_nr):
        phases = self._transformations['transformation_data'][tf_nr]['phases']
        assert len(phases) == 1, "transformation %d in multiple phases! %s" % (tf_nr, phases)
        return list(phases)[0] == 'Obfuscation'

    def BblIsPossiblyFunctionEntry(self, b):
        assert False, "not implemented!"

    def _CollectTransformationInformation(self, ground_truth):
        for i in self.InsIterator():
            if self.InsNotInFunction(i):
                continue

            # skip unconditional branches (may be part of trampoline)
            if self.InsIsUnconditionalBranch(i):
                continue

            # skip Thumb instructions
            if self.InsIsThumb(i):
                self.Print("thumb instruction 0x%x" % i)
                continue

            # transformation UID
            tf_id = ground_truth.InsTransformationID(i)

            # skip instructions created during (de)flowgraph
            if ground_truth.SkipTransformation(tf_id):
                continue

            tf_nr = ground_truth._TransformationIdUID(tf_id)
            if tf_nr not in self._transformations['transformation_data']:
                self._transformations['transformation_data'][tf_nr] = {
                    'instructions': set(),
                    'bbls': set(),
                    'functions': set(),
                    'phases': set()
                }

            self._transformations['transformation_data'][tf_nr]['instructions'].add(i)
            self._transformations['transformation_data'][tf_nr]['bbls'].add(self.InsBbl(i))
            self._transformations['transformation_data'][tf_nr]['functions'].add(self.InsFunction(i))

            phase = ground_truth.InsPhase(i)
            self._transformations['transformation_data'][tf_nr]['phases'].add(phase)
            if phase not in self._transformations['transformation_by_phase']:
                self._transformations['transformation_by_phase'][phase] = set()
            self._transformations['transformation_by_phase'][phase].add(tf_nr)

        tf_nrs_delete = set()
        for tf_nr, data in self._transformations['transformation_data'].items():
            if INVALID_BBL in data['bbls']:
                tf_nrs_delete.add(tf_nr)
        for x in tf_nrs_delete:
            del self._transformations['transformation_data'][x]
        self.Print("eliminated %d transformations with invalid blocks" % (len(tf_nrs_delete)))

    def Print(self, x):
        print("[%s] %s" % (self.__name, x))

    def InsIterator(self):
        for i in self._instructions['instructions']:
            yield i

    def EdgeIterator(self, include_plt=True, skip_to_nonexist_ins=False, skip_calls=True):
        for uid, e in self._edges['edges'].items():
            if not include_plt and e['to_plt']:
                continue

            if skip_calls and e['type'] == EDGE_CALL:
                continue

            # IDA sometimes draws an edge to a non-existent instruction
            # .text:000A155C                 VST1.32         {D6-D7}, [R2]!
            # .text:000A155C ; ---------------------------------------------------------------------------
            # .text:000A1560                 DCD 0xBDB00BDB, 0xBDB00BDB, 0xBDB00BDB, 0xBDB00BDB, 0xF3F04300
            # .text:000A1574 ; ---------------------------------------------------------------------------
            # .text:000A1574
            # .text:000A1574 loc_A1574                               ; CODE XREF: .LVL319_1+179098j
            # .text:000A1574                 VLD1.8          {D16}, [R0]!
            #
            # data at 0xa1560 is not recognised as instruction by IDA
            if skip_to_nonexist_ins and not self.InsExists(e['tail']):
                continue

            yield uid, e

    def BblIterator(self):
        for uid, b in self._bbls.items():
            if not uid in self._instructions['bbls']:
                # skip empty BBLs
                continue

            yield uid, b

    def FunctionIterator(self):
        for uid, f in self._functions.items():
            yield uid, f

    def BblInsIterator(self, b):
        for i in self._instructions['bbls'][b]:
            yield i

    def ObfuscationTransformationIterator(self):
        for nr, data in self._transformations['transformation_data'].items():
            if not self.TransformationIsObfucation(nr):
                continue

            yield nr, data

    def EdgeIsFake(self, e):
        assert False, "EdgeIsFake should be called on the ground truth object!"

    def TransformationData(self, nr):
        return self._transformations['transformation_data'][nr]

    def InsBbl(self, i):
        bbls = self._instructions['instructions'][i]['bbl_uid']
        assert len(bbls) == 1
        return bbls[0]

    def InsAssembled(self, i):
        return self._instructions['instructions'][i]['assembled']

    def BblFunction(self, B):
        return self.InsFunction(B['address'])

    def BblIncomingEdgeIterator(self, b, include_fake=True, skip_calls=True):
        for e in self._bbls[b]['incoming']:
            if not include_fake and self.EdgeIsFake(e):
                continue

            edge = self.EdgeByUID(e)
            if skip_calls and edge['type'] == EDGE_CALL:
                continue

            yield e, edge

    def BblIncomingEdgeSet(self, b):
        result = set()
        for x, _ in self.BblIncomingEdgeIterator(b):
            result.add(x)
        return result

    def _incoming_blocks(self, b, include_fake, skip_trampolines):
        result = set()

        incoming_edges = [x for x in self.BblIncomingEdgeIterator(b, include_fake)]
        for _, ee in incoming_edges:
            x = self.InsBbl(ee['head'])

            if self.BblIsTrampoline(x) and skip_trampolines:
                # self.Print("2 %d (%x) is trampoline" % (x, self._bbls[x]['address']))
                for y in self._incoming_blocks(x, include_fake, skip_trampolines):
                    result.add(y)
            else:
                # self.Print("2 %d (%x) is not trampoline" % (x, self._bbls[x]['address']))
                result.add(x)

        return result

    def BblIncomingBbls(self, b, include_fake=True, skip_trampolines=False, skip_calls=True):
        result = set()
        for _, e in self.BblIncomingEdgeIterator(b, include_fake, skip_calls):
            h = self.InsBbl(e['head'])

            if skip_trampolines and self.BblIsTrampoline(h):
                h = list(self._incoming_blocks(h, include_fake, skip_trampolines))

                for x in h:
                    result.add(x)
            else:
                result.add(h)

            # h = self.InsBbl(e['head'])

            # if skip_trampolines:
            #   while self.BblIsTrampoline(h):
            #     incoming_edges = [x for x in self.BblIncomingEdgeIterator(h, include_fake)]
            #     if len(incoming_edges) == 1:
            #       for _, ee in incoming_edges:
            #         h = self.InsBbl(ee['head'])

            #     else:
            #       break

            # result.add(h)

        return result

    def BblOutgoingEdgeIterator(self, b, include_fake=True, skip_calls=True):
        insns = self._instructions['bbls'][b]
        last_insn = sorted(insns)[-1]

        if last_insn not in self._edges['outgoing']:
            # no outgoing edges
            return

        for edge_uid in self._edges['outgoing'][last_insn]:
            if not include_fake and self.EdgeIsFake(edge_uid):
                continue

            edge = self.EdgeByUID(edge_uid)
            if not self.InsExists(edge['tail']):
                continue
            if skip_calls and edge['type'] == EDGE_CALL:
                continue

            yield edge_uid, edge

    def BblOutgoingEdgeSet(self, b):
        result = set()
        for x, _ in self.BblOutgoingEdgeIterator(b):
            result.add(x)
        return result

    def BblOutgoingBbls(self, b, include_fake=True, skip_trampolines=False):
        result = set()
        for x, e in self.BblOutgoingEdgeIterator(b, include_fake):
            t = self.InsBbl(e['tail'])

            if skip_trampolines:
                trampolines_done = set()
                while self.BblIsTrampoline(t):
                    trampolines_done.add(t)
                    for x, ee in self.BblOutgoingEdgeIterator(t):
                        t = self.InsBbl(ee['tail'])
                        break

                    if t in trampolines_done:
                        break

            result.add(t)
        return result

    def BblIsTrampoline(self, b):
        # only BBLs having one instruction are candidates
        if self._bbls[b]['nins'] != 1:
            return False

        for ins in self.BblInsIterator(b):
            return self.InsIsUnconditionalBranch(ins)

    def EdgeByUID(self, e):
        return self._edges['edges'][e]

    def BblByUID(self, b):
        return self._bbls[b]

    def BblAddress(self, b):
        return self.BblByUID(b)['address']

    def InsFunction(self, i):
        functions = self._instructions['instructions'][i]['function_uid']
        assert len(functions) == 1
        return functions[0]

    def InsMoveToFunction(self, i, f):
        assert len(self._instructions['instructions'][i]['function_uid']) == 1
        self._instructions['instructions'][i]['function_uid'] = [f]

    def InsExists(self, i):
        return i in self._instructions['instructions']

    def FunctionSize(self, f):
        return self._functions[f]['nr_bbls']

    def InsCount(self):
        return len(self._instructions['instructions'])

    def BblCount(self):
        return len(self._bbls)

    def FunctionCount(self):
        return len(self._functions)

    def EdgeCount(self):
        return len(self._edges['edges'])

    def InsCountNotInFunction(self):
        result = 0
        if -1 in self._instructions['functions']:
            result = len(self._instructions['functions'][-1])
        return result

    def InsNotInFunction(self, i):
        return (-1 in self._instructions['functions']) and (i in self._instructions['functions'][-1])

    def TransformationCount(self):
        return len(self._transformations['transformation_data'])

    def InsInSameBbl(self, i1, i2):
        if not self.InsExists(i1) or not self.InsExists(i2):
            return False

        b1 = self.InsBbl(i1)
        b2 = self.InsBbl(i2)

        if (b1 == INVALID_BBL) or (b2 == INVALID_BBL):
            return False

        return b1 == b2

    def EdgeToString(self, edge_uid):
        edge = self.EdgeByUID(edge_uid)
        return "[%d] 0x%x (0x%x) -> 0x%x (%d)" % (edge_uid, edge['head'], edge['branch'], edge['tail'], edge['type'])
