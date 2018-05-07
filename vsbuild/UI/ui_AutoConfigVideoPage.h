/********************************************************************************
** Form generated from reading UI file 'AutoConfigVideoPage.ui'
**
** Created by: Qt User Interface Compiler version 5.7.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_AUTOCONFIGVIDEOPAGE_H
#define UI_AUTOCONFIGVIDEOPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AutoConfigVideoPage
{
public:
    QVBoxLayout *verticalLayout;
    QFormLayout *formLayout_2;
    QLabel *label;
    QComboBox *canvasRes;
    QLabel *label_3;
    QComboBox *fps;
    QSpacerItem *horizontalSpacer;
    QLabel *warningLabel;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *AutoConfigVideoPage)
    {
        if (AutoConfigVideoPage->objectName().isEmpty())
            AutoConfigVideoPage->setObjectName(QStringLiteral("AutoConfigVideoPage"));
        AutoConfigVideoPage->resize(470, 300);
        AutoConfigVideoPage->setWindowTitle(QStringLiteral(""));
        verticalLayout = new QVBoxLayout(AutoConfigVideoPage);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        formLayout_2 = new QFormLayout();
        formLayout_2->setObjectName(QStringLiteral("formLayout_2"));
        formLayout_2->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
        formLayout_2->setLabelAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label = new QLabel(AutoConfigVideoPage);
        label->setObjectName(QStringLiteral("label"));

        formLayout_2->setWidget(0, QFormLayout::LabelRole, label);

        canvasRes = new QComboBox(AutoConfigVideoPage);
        canvasRes->setObjectName(QStringLiteral("canvasRes"));

        formLayout_2->setWidget(0, QFormLayout::FieldRole, canvasRes);

        label_3 = new QLabel(AutoConfigVideoPage);
        label_3->setObjectName(QStringLiteral("label_3"));

        formLayout_2->setWidget(2, QFormLayout::LabelRole, label_3);

        fps = new QComboBox(AutoConfigVideoPage);
        fps->setObjectName(QStringLiteral("fps"));

        formLayout_2->setWidget(2, QFormLayout::FieldRole, fps);

        horizontalSpacer = new QSpacerItem(87, 17, QSizePolicy::Fixed, QSizePolicy::Minimum);

        formLayout_2->setItem(3, QFormLayout::LabelRole, horizontalSpacer);


        verticalLayout->addLayout(formLayout_2);

        warningLabel = new QLabel(AutoConfigVideoPage);
        warningLabel->setObjectName(QStringLiteral("warningLabel"));
        warningLabel->setWordWrap(true);

        verticalLayout->addWidget(warningLabel);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

#ifndef QT_NO_SHORTCUT
        label->setBuddy(canvasRes);
        label_3->setBuddy(fps);
#endif // QT_NO_SHORTCUT

        retranslateUi(AutoConfigVideoPage);

        QMetaObject::connectSlotsByName(AutoConfigVideoPage);
    } // setupUi

    void retranslateUi(QWidget *AutoConfigVideoPage)
    {
        label->setText(QApplication::translate("AutoConfigVideoPage", "Basic.Settings.Video.BaseResolution", 0));
        label_3->setText(QApplication::translate("AutoConfigVideoPage", "Basic.Settings.Video.FPS", 0));
        warningLabel->setText(QApplication::translate("AutoConfigVideoPage", "Basic.AutoConfig.VideoPage.CanvasExplanation", 0));
        Q_UNUSED(AutoConfigVideoPage);
    } // retranslateUi

};

namespace Ui {
    class AutoConfigVideoPage: public Ui_AutoConfigVideoPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_AUTOCONFIGVIDEOPAGE_H
