# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'InstructionInfoDock.ui'
##
## Created by: Qt User Interface Compiler version 5.15.2
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import *
from PySide6.QtGui import *
from PySide6.QtWidgets import *


class Ui_InstructionInfoDock(object):
    def setupUi(self, InstructionInfoDock):
        if not InstructionInfoDock.objectName():
            InstructionInfoDock.setObjectName(u"InstructionInfoDock")
        InstructionInfoDock.resize(834, 510)
        self.verticalLayout = QVBoxLayout(InstructionInfoDock)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.groupBox = QGroupBox(InstructionInfoDock)
        self.groupBox.setObjectName(u"groupBox")
        self.formLayout = QFormLayout(self.groupBox)
        self.formLayout.setObjectName(u"formLayout")
        self.label = QLabel(self.groupBox)
        self.label.setObjectName(u"label")

        self.formLayout.setWidget(0, QFormLayout.LabelRole, self.label)

        self.label_2 = QLabel(self.groupBox)
        self.label_2.setObjectName(u"label_2")

        self.formLayout.setWidget(1, QFormLayout.LabelRole, self.label_2)

        self.read_values_tablewidget = QTableWidget(self.groupBox)
        self.read_values_tablewidget.setObjectName(u"read_values_tablewidget")
        self.read_values_tablewidget.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.read_values_tablewidget.setSelectionMode(QAbstractItemView.NoSelection)
        self.read_values_tablewidget.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.read_values_tablewidget.setSortingEnabled(True)

        self.formLayout.setWidget(0, QFormLayout.FieldRole, self.read_values_tablewidget)

        self.written_values_tablewidget = QTableWidget(self.groupBox)
        self.written_values_tablewidget.setObjectName(u"written_values_tablewidget")
        self.written_values_tablewidget.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.written_values_tablewidget.setSelectionMode(QAbstractItemView.NoSelection)
        self.written_values_tablewidget.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.written_values_tablewidget.setSortingEnabled(True)

        self.formLayout.setWidget(1, QFormLayout.FieldRole, self.written_values_tablewidget)


        self.verticalLayout.addWidget(self.groupBox)

        self.groupBox_2 = QGroupBox(InstructionInfoDock)
        self.groupBox_2.setObjectName(u"groupBox_2")
        self.horizontalLayout = QHBoxLayout(self.groupBox_2)
        self.horizontalLayout.setObjectName(u"horizontalLayout")
        self.highlight_deps_checkbox = QCheckBox(self.groupBox_2)
        self.highlight_deps_checkbox.setObjectName(u"highlight_deps_checkbox")
        self.highlight_deps_checkbox.setChecked(True)

        self.horizontalLayout.addWidget(self.highlight_deps_checkbox)


        self.verticalLayout.addWidget(self.groupBox_2)


        self.retranslateUi(InstructionInfoDock)

        QMetaObject.connectSlotsByName(InstructionInfoDock)
    # setupUi

    def retranslateUi(self, InstructionInfoDock):
        InstructionInfoDock.setWindowTitle(QCoreApplication.translate("InstructionInfoDock", u"Form", None))
        self.groupBox.setTitle(QCoreApplication.translate("InstructionInfoDock", u"Values", None))
        self.label.setText(QCoreApplication.translate("InstructionInfoDock", u"Read values:", None))
        self.label_2.setText(QCoreApplication.translate("InstructionInfoDock", u"Written values:", None))
        self.groupBox_2.setTitle(QCoreApplication.translate("InstructionInfoDock", u"Data dependencies", None))
        self.highlight_deps_checkbox.setText(QCoreApplication.translate("InstructionInfoDock", u"Highlight dependencies", None))
    # retranslateUi

