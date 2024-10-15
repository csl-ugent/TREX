# -*- coding: utf-8 -*-

################################################################################
## Form generated from reading UI file 'MBADetectDock.ui'
##
## Created by: Qt User Interface Compiler version 6.2.4
##
## WARNING! All changes made in this file will be lost when recompiling UI file!
################################################################################

from PySide6.QtCore import (QCoreApplication, QDate, QDateTime, QLocale,
    QMetaObject, QObject, QPoint, QRect,
    QSize, QTime, QUrl, Qt)
from PySide6.QtGui import (QBrush, QColor, QConicalGradient, QCursor,
    QFont, QFontDatabase, QGradient, QIcon,
    QImage, QKeySequence, QLinearGradient, QPainter,
    QPalette, QPixmap, QRadialGradient, QTransform)
from PySide6.QtWidgets import (QAbstractItemView, QApplication, QFormLayout, QGroupBox,
    QHeaderView, QSizePolicy, QTableWidget, QTableWidgetItem,
    QVBoxLayout, QWidget)

class Ui_MBADetectDock(object):
    def setupUi(self, MBADetectDock):
        if not MBADetectDock.objectName():
            MBADetectDock.setObjectName(u"MBADetectDock")
        MBADetectDock.resize(710, 330)
        self.verticalLayout = QVBoxLayout(MBADetectDock)
        self.verticalLayout.setObjectName(u"verticalLayout")
        self.groupBox = QGroupBox(MBADetectDock)
        self.groupBox.setObjectName(u"groupBox")
        self.formLayout = QFormLayout(self.groupBox)
        self.formLayout.setObjectName(u"formLayout")
        self.mba_expressions_tablewidget = QTableWidget(self.groupBox)
        if (self.mba_expressions_tablewidget.columnCount() < 3):
            self.mba_expressions_tablewidget.setColumnCount(3)
        __qtablewidgetitem = QTableWidgetItem()
        self.mba_expressions_tablewidget.setHorizontalHeaderItem(0, __qtablewidgetitem)
        __qtablewidgetitem1 = QTableWidgetItem()
        self.mba_expressions_tablewidget.setHorizontalHeaderItem(1, __qtablewidgetitem1)
        __qtablewidgetitem2 = QTableWidgetItem()
        self.mba_expressions_tablewidget.setHorizontalHeaderItem(2, __qtablewidgetitem2)
        self.mba_expressions_tablewidget.setObjectName(u"mba_expressions_tablewidget")
        self.mba_expressions_tablewidget.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.mba_expressions_tablewidget.setSelectionMode(QAbstractItemView.SingleSelection)
        self.mba_expressions_tablewidget.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.mba_expressions_tablewidget.setHorizontalScrollMode(QAbstractItemView.ScrollPerPixel)
        self.mba_expressions_tablewidget.horizontalHeader().setCascadingSectionResizes(True)
        self.mba_expressions_tablewidget.horizontalHeader().setStretchLastSection(True)

        self.formLayout.setWidget(0, QFormLayout.SpanningRole, self.mba_expressions_tablewidget)


        self.verticalLayout.addWidget(self.groupBox)


        self.retranslateUi(MBADetectDock)

        QMetaObject.connectSlotsByName(MBADetectDock)
    # setupUi

    def retranslateUi(self, MBADetectDock):
        MBADetectDock.setWindowTitle(QCoreApplication.translate("MBADetectDock", u"Form", None))
        self.groupBox.setTitle(QCoreApplication.translate("MBADetectDock", u"MBA expressions", None))
        ___qtablewidgetitem = self.mba_expressions_tablewidget.horizontalHeaderItem(0)
        ___qtablewidgetitem.setText(QCoreApplication.translate("MBADetectDock", u"#", None));
        ___qtablewidgetitem1 = self.mba_expressions_tablewidget.horizontalHeaderItem(1)
        ___qtablewidgetitem1.setText(QCoreApplication.translate("MBADetectDock", u"Complex expression", None));
        ___qtablewidgetitem2 = self.mba_expressions_tablewidget.horizontalHeaderItem(2)
        ___qtablewidgetitem2.setText(QCoreApplication.translate("MBADetectDock", u"Simple expression", None));
    # retranslateUi

