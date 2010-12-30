#include "licensewizard.h"

#include <QApplication>

#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QTextEdit>

#include <QVBoxLayout>

#include <QMessageBox>
#include <QRegExpValidator> //email validator

#include <QFile>

#include <QPrintDialog>
#include <QPrinter>

#include "appinfo.h" // email RegEx & licensePage
#include "licensevalidator.h"

#include "settings.h"

#include <QDebug>

/*FIXME: Add network connection to transfer the data to the website.
 *The network connection will look up the base url from the config file
 *the settings class will default to the official website, but if the
 *user changes the setting in the config file this wizard will use that setting.
 */
LicenseWizard::LicenseWizard(QWidget *parent)
    : QWizard(parent)
{
    setPage(Page_Intro, new IntroPage);
    setPage(Page_Evaluate, new EvaluatePage);
    setPage(Page_Register, new RegisterPage);
    setPage(Page_Conclusion, new ConclusionPage);

    setStartId(Page_Intro);

#ifndef Q_WS_MAC
    setWizardStyle(ModernStyle);
#endif
    setOption(HaveHelpButton, true);
    setPixmap(QWizard::LogoPixmap, QPixmap(":/images/logo.png"));

    connect(this, SIGNAL(helpRequested()), this, SLOT(showHelp()));

    setWindowTitle(tr("License Wizard"));
}

void LicenseWizard::showHelp()
{
    static QString lastHelpMessage;

    QString message;

    switch (currentId()) {
        case Page_Intro:
            message = tr("The decision you make here will affect which page you get to see next.");
            break;
        case Page_Evaluate:
            message = tr("Make sure to provide a valid email address, such as toni.buddenbrook@example.de.");
            break;
        case Page_Register:
            message = tr("If you don't provide an upgrade key, you will be asked to fill in your details.");
            break;
        case Page_Conclusion:
            message = tr("You must accept the terms and conditions of the license to proceed.");
            break;
        default:
            message = tr("This help is likely not to be of any help.");
    }

    QMessageBox::information(this, tr("License Wizard Help"), message);
    lastHelpMessage = message;
}

IntroPage::IntroPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Introduction"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/wizard_watermark.png"));

    topLabel = new QLabel(tr("This wizard will walk you through the registration of "
                             "<i>%1</i>&trade;.").arg(qApp->applicationName()));
    topLabel->setWordWrap(true);

    registerRadioButton = new QRadioButton(tr("&Register your copy"));
    evaluateRadioButton = new QRadioButton(tr("&Evaluate for 30 days"));
    registerRadioButton->setChecked(true);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(topLabel);
    layout->addWidget(registerRadioButton);
    layout->addWidget(evaluateRadioButton);
    setLayout(layout);
}

int IntroPage::nextId() const
{
    if (evaluateRadioButton->isChecked()) {
        return LicenseWizard::Page_Evaluate;
    } else {
        return LicenseWizard::Page_Register;
    }
}

EvaluatePage::EvaluatePage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Evaluate <i>%1</i>&trade;").arg(qApp->applicationName()));
    setSubTitle(tr("Please fill both fields. Make sure to provide a valid "
                   "email address (e.g., john.smith@example.com)."));

    nameLabel = new QLabel(tr("N&ame:"));
    nameLineEdit = new QLineEdit;
    nameLabel->setBuddy(nameLineEdit);

    emailLabel = new QLabel(tr("&Email address:"));
    emailLineEdit = new QLineEdit;
    emailLineEdit->setValidator(new QRegExpValidator(AppInfo::emailRegExp, this));
    emailLabel->setBuddy(emailLineEdit);

    registerField("evaluate.name*", nameLineEdit);
    registerField("evaluate.email*", emailLineEdit);

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(nameLabel, 0, 0);
    layout->addWidget(nameLineEdit, 0, 1);
    layout->addWidget(emailLabel, 1, 0);
    layout->addWidget(emailLineEdit, 1, 1);
    setLayout(layout);
}

int EvaluatePage::nextId() const
{
    return LicenseWizard::Page_Conclusion;
}

RegisterPage::RegisterPage(QWidget *parent)
    : QWizardPage(parent)
{
    mAllowNextPage = false;
    mLicHttp = new LicenseHttp(this);

    setTitle(tr("Register Your Copy of <i>%1</i>&trade;").arg(qApp->applicationName()));
    setSubTitle(tr("Please fill in all the fields."));

    nameLabel = new QLabel(tr("N&ame:"));
    nameLineEdit = new QLineEdit(this);
    nameLabel->setBuddy(nameLineEdit);

    emailLabel = new QLabel(tr("&Email:"));
    emailLineEdit = new QLineEdit(this);
    emailLabel->setBuddy(emailLineEdit);
    emailLineEdit->setValidator(new QRegExpValidator(AppInfo::emailRegExp, this));

    serialNumberLabel = new QLabel(tr("&Serial Number:"));
    serialNumberLineEdit = new QLineEdit(this);
    serialNumberLabel->setBuddy(serialNumberLineEdit);

    serialNumberLineEdit->setValidator(new LicenseValidator(this));

    licenseNumberLineEdit = new QLineEdit(this);
    licenseNumberLineEdit->setVisible(false);

    registerField("register.name*", nameLineEdit);
    registerField("register.email*", emailLineEdit);
    registerField("register.serialNumber*", serialNumberLineEdit);

    registerField("register.license", licenseNumberLineEdit);

    QGridLayout *layout = new QGridLayout;
    layout->addWidget(nameLabel, 0, 0);
    layout->addWidget(nameLineEdit, 0, 1);
    layout->addWidget(emailLabel, 1, 0);
    layout->addWidget(emailLineEdit, 1, 1);
    layout->addWidget(serialNumberLabel, 2, 0);
    layout->addWidget(serialNumberLineEdit, 2, 1);
    setLayout(layout);
}

bool RegisterPage::validatePage()
{
    //Look up the licensePage value so I can use a testing server if I need to, otherwise it
    //should always default to the live server as specified in AppInfo::licensePage;
    QString path = Settings::inst()->value("licensePage", QVariant(AppInfo::liveLicensePage)).toString();
    path = QString(path).arg(serialNumberLineEdit->text()).arg(emailLineEdit->text()).arg(nameLineEdit->text()).arg(nameLineEdit->text());
    QUrl url(path);

    mLicHttp->downloadFile(url);
    connect(mLicHttp, SIGNAL(licenseCompleted(QString,bool)), this, SLOT(getLicense(QString,bool)));

    return mAllowNextPage;
}

void RegisterPage::getLicense(QString license, bool errors)
{
    if(errors) {
        mAllowNextPage = false;
        QMessageBox::information(this, "Error", "Unable to register with the server.");
    }

    setField("register.license", QVariant(license));
    mAllowNextPage = true;
    this->wizard()->next();
}

int RegisterPage::nextId() const
{
    return LicenseWizard::Page_Conclusion;
}

ConclusionPage::ConclusionPage(QWidget *parent)
    : QWizardPage(parent)
{
    setTitle(tr("Complete Your Registration"));
    setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/wizard_watermark.png"));

    licenseEdit = new QTextEdit;

    agreeCheckBox = new QCheckBox(tr("I agree to the terms of the license"));

    registerField("conclusion.agree*", agreeCheckBox);

    QVBoxLayout *layout = new QVBoxLayout;
    layout->addWidget(licenseEdit);
    layout->addWidget(agreeCheckBox);
    setLayout(layout);
}

int ConclusionPage::nextId() const
{
    return -1;
}

void ConclusionPage::initializePage()
{
    QFile f(":/license.txt");
    if(!f.open(QIODevice::ReadOnly))
        return;
    QString licenseText = f.readAll();

    if (wizard()->hasVisitedPage(LicenseWizard::Page_Evaluate)) {
/*        licenseText = tr("<u>Evaluation License Agreement:</u> "
                         "You can use this software for 30 days and make one "
                         "backup, but you are not allowed to distribute it.");
*/
    } else if (wizard()->hasVisitedPage(LicenseWizard::Page_Register)) {
/*        licenseText = tr("<u>First-Time License Agreement:</u> "
                         "You can use this software subject to the license "
                         "you will receive by email.");
*/
    } else {
/*        licenseText = tr("<u>Upgrade License Agreement:</u> "
                         "This software is licensed under the terms of your "
                         "current license.");
*/
    }
    licenseEdit->setHtml(licenseText);
    licenseEdit->setReadOnly(true);

    QString name  = field("register.name").toString();
    QString email = field("register.email").toString();
    QString sn    = field("register.serialNumber").toString();

    Settings::inst()->setValue("name", QVariant(name));
    Settings::inst()->setValue("email", QVariant(email));
    Settings::inst()->setValue("serialNumber", QVariant(sn));
}

void ConclusionPage::setVisible(bool visible)
{
    QWizardPage::setVisible(visible);

    if (visible) {
        wizard()->setButtonText(QWizard::CustomButton1, tr("&Print"));
        wizard()->setOption(QWizard::HaveCustomButton1, true);
        connect(wizard(), SIGNAL(customButtonClicked(int)),
                this, SLOT(printButtonClicked()));
    } else {
        wizard()->setOption(QWizard::HaveCustomButton1, false);
        disconnect(wizard(), SIGNAL(customButtonClicked(int)),
                   this, SLOT(printButtonClicked()));
    }
}

void ConclusionPage::printButtonClicked()
{
    QPrinter printer;
    QPrintDialog dialog(&printer, this);
    if (dialog.exec())
        licenseEdit->print(&printer);
}

void ConclusionPage::cleanupPage()
{
    //the back button has been pressed, clear the data entered...
    Settings::inst()->setValue("name", "");
    Settings::inst()->setValue("email", "");
    Settings::inst()->setValue("serialNumber", "");
}
