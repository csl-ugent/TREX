# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'MemoryBufferInfoDock.ui'
##
## Created by: Qt User Interface Compiler version 5.15.2
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import *
from PySide6.QtGui import *
from PySide6.QtWidgets import *


class Ui_MemoryBufferInfoDock(object):
    def setupUi(self, MemoryBufferInfoDock):
        if not MemoryBufferInfoDock.objectName():
            MemoryBufferInfoDock.setObjectName(u"MemoryBufferInfoDock")
        MemoryBufferInfoDock.resize(877, 656)
        self.verticalLayout = QVBoxLayout(MemoryBufferInfoDock)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.groupBox = QGroupBox(MemoryBufferInfoDock)
        self.groupBox.setObjectName(u"groupBox")
        self.verticalLayout_2 = QVBoxLayout(self.groupBox)
        self.verticalLayout_2.setObjectName(u"verticalLayout_2")
        self.current_instruction_checkbox = QCheckBox(self.groupBox)
        self.current_instruction_checkbox.setObjectName(u"current_instruction_checkbox")
        self.current_instruction_checkbox.setChecked(True)

        self.verticalLayout_2.addWidget(self.current_instruction_checkbox)

        self.memory_buffers_tablewidget = QTableWidget(self.groupBox)
        self.memory_buffers_tablewidget.setObjectName(u"memory_buffers_tablewidget")
        self.memory_buffers_tablewidget.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.memory_buffers_tablewidget.setSelectionMode(QAbstractItemView.SingleSelection)
        self.memory_buffers_tablewidget.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.memory_buffers_tablewidget.setIconSize(QSize(32, 32))
        self.memory_buffers_tablewidget.setSortingEnabled(True)

        self.verticalLayout_2.addWidget(self.memory_buffers_tablewidget)


        self.verticalLayout.addWidget(self.groupBox)

        self.groupBox_2 = QGroupBox(MemoryBufferInfoDock)
        self.groupBox_2.setObjectName(u"groupBox_2")
        self.verticalLayout_3 = QVBoxLayout(self.groupBox_2)
        self.verticalLayout_3.setObjectName(u"verticalLayout_3")
        self.references_tablewidget = QTableWidget(self.groupBox_2)
        self.references_tablewidget.setObjectName(u"references_tablewidget")
        self.references_tablewidget.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.references_tablewidget.setSelectionMode(QAbstractItemView.SingleSelection)
        self.references_tablewidget.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.references_tablewidget.setIconSize(QSize(32, 32))
        self.references_tablewidget.setSortingEnabled(True)

        self.verticalLayout_3.addWidget(self.references_tablewidget)


        self.verticalLayout.addWidget(self.groupBox_2)


        self.retranslateUi(MemoryBufferInfoDock)

        QMetaObject.connectSlotsByName(MemoryBufferInfoDock)
    # setupUi

    def retranslateUi(self, MemoryBufferInfoDock):
        MemoryBufferInfoDock.setWindowTitle(QCoreApplication.translate("MemoryBufferInfoDock", u"Form", None))
        self.groupBox.setTitle(QCoreApplication.translate("MemoryBufferInfoDock", u"Memory buffer list", None))
        self.current_instruction_checkbox.setText(QCoreApplication.translate("MemoryBufferInfoDock", u"Limit to current instruction only", None))
        self.groupBox_2.setTitle(QCoreApplication.translate("MemoryBufferInfoDock", u"Memory buffer references", None))
    # retranslateUi

