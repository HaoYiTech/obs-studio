/********************************************************************************
** Form generated from reading UI file 'AutoConfigStartPage.ui'
**
** Created by: Qt User Interface Compiler version 5.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_AUTOCONFIGSTARTPAGE_H
#define UI_AUTOCONFIGSTARTPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AutoConfigStartPage
{
public:
    QVBoxLayout *verticalLayout;
    QRadioButton *prioritizeStreaming;
    QRadioButton *prioritizeRecording;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *AutoConfigStartPage)
    {
        if (AutoConfigStartPage->objectName().isEmpty())
            AutoConfigStartPage->setObjectName(QStringLiteral("AutoConfigStartPage"));
        AutoConfigStartPage->resize(400, 300);
        AutoConfigStartPage->setWindowTitle(QStringLiteral(""));
        verticalLayout = new QVBoxLayout(AutoConfigStartPage);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        prioritizeStreaming = new QRadioButton(AutoConfigStartPage);
        prioritizeStreaming->setObjectName(QStringLiteral("prioritizeStreaming"));
        prioritizeStreaming->setChecked(true);

        verticalLayout->addWidget(prioritizeStreaming);

        prioritizeRecording = new QRadioButton(AutoConfigStartPage);
        prioritizeRecording->setObjectName(QStringLiteral("prioritizeRecording"));

        verticalLayout->addWidget(prioritizeRecording);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        retranslateUi(AutoConfigStartPage);

        QMetaObject::connectSlotsByName(AutoConfigStartPage);
    } // setupUi

    void retranslateUi(QWidget *AutoConfigStartPage)
    {
        prioritizeStreaming->setText(QApplication::translate("AutoConfigStartPage", "Basic.AutoConfig.StartPage.PrioritizeStreaming", 0));
        prioritizeRecording->setText(QApplication::translate("AutoConfigStartPage", "Basic.AutoConfig.StartPage.PrioritizeRecording", 0));
        Q_UNUSED(AutoConfigStartPage);
    } // retranslateUi

};

namespace Ui {
    class AutoConfigStartPage: public Ui_AutoConfigStartPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_AUTOCONFIGSTARTPAGE_H
