from typing import Dict, List
from mate_attack_framework.ui.ui_mbadetectdock import Ui_MBADetectDock
from mate_attack_framework.docks.dockbase import DockBase
from mate_attack_framework.util import set_headers, set_items, get_database, get_image_name
from mate_attack_framework.mba.highlight import highlight_mba_instructions, attach_comment_to_mba_instructions, clear_mba_comments
import neo4j

class MBADetectDock(DockBase):
    def __init__(self, parent, name) -> None:
        DockBase.__init__(self, parent, name, ui = Ui_MBADetectDock())

        self._detected_expressions = self._get_mba_expressions()

        set_headers(self.ui.mba_expressions_tablewidget, ['#', 'Complex expression', 'Simple expression'])
        items = [
            [str(idx), expr["complex_expression"], expr["simple_expression"]] for idx, expr in enumerate(self._detected_expressions)
        ]
        set_items(self.ui.mba_expressions_tablewidget, items)
        self.ui.mba_expressions_tablewidget.cellClicked.connect(self._mba_expression_cell_selected)

        self._comments_attached = False
    
    def _mba_expression_cell_selected(self, row, col):
        expr = self._detected_expressions[row]

        highlight_mba_instructions(self.binary_view, expr["addresses"])

        lowest_expr_addr = min(expr["addresses"])

        self.binary_view.navigate(self.binary_view.view, lowest_expr_addr)

    
    def _get_mba_expressions(self) -> List[Dict]:
        db = get_database()

        query = f'''
            MATCH (mba:BinaryExpression)-[CONTAINS_INSTRUCTION]->(insn:Instruction)
            RETURN mba, insn'''

        cursor:List = db.run_list(query)

        res = {}

        item:neo4j.Record
        for item in cursor:
            expr_node:neo4j.graph.Node = item.get("mba")
            insn_node:neo4j.graph.Node = item.get("insn")
            
            tmp_expr_node_id = expr_node.element_id # can change accross transactions, but should suffice to identify all records corresponding to the same expression
            complex_expression = expr_node.get("complex_expression")
            simple_expression = expr_node.get("simple_expression")

            addr = insn_node.get("image_offset")

            if not tmp_expr_node_id in res:
                res[tmp_expr_node_id] = {
                    "complex_expression": complex_expression,
                    "simple_expression": simple_expression,
                    "addresses": [],
                }
            
            res[tmp_expr_node_id]["addresses"].append(addr)

        return list(res.values())
    
    def notifyViewChanged(self, view_frame):
        super().notifyViewChanged(view_frame)

        if self.binary_view is not None:
            if not self._comments_attached:
                # clear_mba_comments(self.binary_view)

                for idx, mba_expr in enumerate(self._detected_expressions):
                    attach_comment_to_mba_instructions(self.binary_view, mba_expr["addresses"], "MBA #{}".format(idx))
                
                self._comments_attached = True