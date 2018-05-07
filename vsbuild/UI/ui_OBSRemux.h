/********************************************************************************
** Form generated from reading UI file 'OBSRemux.ui'
**
** Created by: Qt User Interface Compiler version 5.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OBSREMUX_H
#define UI_OBSREMUX_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_OBSRemux
{
public:
    QGridLayout *formLayout;
    QLabel *label;
    QLabel *label_2;
    QHBoxLayout *horizontalLayout_2;
    QLineEdit *sourceFile;
    QPushButton *browseSource;
    QHBoxLayout *horizontalLayout_3;
    QLineEdit *targetFile;
    QPushButton *browseTarget;
    QProgressBar *progressBar;
    QHBoxLayout *horizontalLayout_4;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *OBSRemux)
    {
        if (OBSRemux->objectName().isEmpty())
            OBSRemux->setObjectName(QStringLiteral("OBSRemux"));
        OBSRemux->resize(491, 124);
        formLayout = new QGridLayout(OBSRemux);
        formLayout->setObjectName(QStringLiteral("formLayout"));
        label = new QLabel(OBSRemux);
        label->setObjectName(QStringLiteral("label"));

        formLayout->addWidget(label, 1, 0, 1, 1);

        label_2 = new QLabel(OBSRemux);
        label_2->setObjectName(QStringLiteral("label_2"));

        formLayout->addWidget(label_2, 2, 0, 1, 1);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        sourceFile = new QLineEdit(OBSRemux);
        sourceFile->setObjectName(QStringLiteral("sourceFile"));

        horizontalLayout_2->addWidget(sourceFile);

        browseSource = new QPushButton(OBSRemux);
        browseSource->setObjectName(QStringLiteral("browseSource"));

        horizontalLayout_2->addWidget(browseSource);


        formLayout->addLayout(horizontalLayout_2, 1, 1, 1, 1);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        targetFile = new QLineEdit(OBSRemux);
        targetFile->setObjectName(QStringLiteral("targetFile"));

        horizontalLayout_3->addWidget(targetFile);

        browseTarget = new QPushButton(OBSRemux);
        browseTarget->setObjectName(QStringLiteral("browseTarget"));

        horizontalLayout_3->addWidget(browseTarget);


        formLayout->addLayout(horizontalLayout_3, 2, 1, 1, 1);

        progressBar = new QProgressBar(OBSRemux);
        progressBar->setObjectName(QStringLiteral("progressBar"));
        progressBar->setValue(24);

        formLayout->addWidget(progressBar, 3, 0, 1, 2);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName(QStringLiteral("horizontalLayout_4"));
        buttonBox = new QDialogButtonBox(OBSRemux);
        buttonBox->setObjectName(QStringLiteral("buttonBox"));
        buttonBox->setStandardButtons(QDialogButtonBox::Ok|QDialogButtonBox::Close);

        horizontalLayout_4->addWidget(buttonBox);


        formLayout->addLayout(horizontalLayout_4, 4, 1, 1, 1);


        retranslateUi(OBSRemux);

        QMetaObject::connectSlotsByName(OBSRemux);
    } // setupUi

    void retranslateUi(QDialog *OBSRemux)
    {
        OBSRemux->setWindowTitle(QApplication::translate("OBSRemux", "RemuxRecordings", 0));
        label->setText(QApplication::translate("OBSRemux", "Remux.SourceFile", 0));
        label_2->setText(QApplication::translate("OBSRemux", "Remux.TargetFile", 0));
        browseSource->setText(QApplication::translate("OBSRemux", "Browse", 0));
        browseTarget->setText(QApplication::translate("OBSRemux", "Browse", 0));
    } // retranslateUi

};

namespace Ui {
    class OBSRemux: public Ui_OBSRemux {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OBSREMUX_H
