#include "SetupWizard.h"
#include "SetupWizard_p.h"
#include "core/BluecherryApp.h"
#include "core/DVRServer.h"
#include <QLabel>
#include <QBoxLayout>
#include <QGridLayout>
#include <QLineEdit>
#include <QCheckBox>
#include <QIntValidator>
#include <QMovie>
#include <QVariant>
#include <QNetworkReply>
#include <QHideEvent>

SetupWizard::SetupWizard(QWidget *parent)
    : QWizard(parent), skipFlag(false)
{
    setWindowTitle(tr("Bluecherry - Setup"));

    addPage(new SetupWelcomePage);
    addPage(new SetupServerPage);
    addPage(new SetupFinishPage);
}

bool SetupWizard::validateCurrentPage()
{
    bool ok = QWizard::validateCurrentPage();

    if (ok && !skipFlag && qobject_cast<SetupServerPage*>(currentPage()))
        static_cast<SetupServerPage*>(currentPage())->save();

    return ok;
}

void SetupWizard::skip()
{
    skipFlag = true;
    next();
    skipFlag = false;
}

SetupWelcomePage::SetupWelcomePage()
{
    setTitle(tr("Welcome"));
    setSubTitle(tr("Welcome to the Bluecherry Surveillance DVR! This wizard will help you connect "
                   "to your DVR server and get started."));

    QBoxLayout *layout = new QVBoxLayout(this);
    layout->addStretch();

    QLabel *logo = new QLabel;
    logo->setPixmap(QPixmap(QLatin1String(":/images/logo.png")));
    logo->setAlignment(Qt::AlignHCenter | Qt::AlignBottom);
    layout->addWidget(logo, 0, Qt::AlignHCenter | Qt::AlignBottom);
}

SetupServerPage::SetupServerPage()
    : loginReply(0), saved(false)
{
    loginRequestTimer.setSingleShot(true);
    connect(&loginRequestTimer, SIGNAL(timeout()), SLOT(testLogin()));

    setTitle(tr("Configure a DVR Server"));
    setSubTitle(tr("Setup a connection to your remote DVR server. You can connect to any number of "
                   "servers, from anywhere in the world."));
    setCommitPage(true);
    setButtonText(QWizard::CommitButton, tr("Finish"));
    setButtonText(QWizard::CustomButton1, tr("Skip"));

    QGridLayout *layout = new QGridLayout(this);

    int row = 0;

    layout->setRowMinimumHeight(row, 16);

    row++;
    QLineEdit *hostEdit = new QLineEdit;
    layout->addWidget(new QLabel(tr("Hostname:")), row, 0);
    layout->addWidget(hostEdit, row, 1);

    QLineEdit *portEdit = new QLineEdit;
    portEdit->setValidator(new QIntValidator(1, 65535, portEdit));
    portEdit->setText(QLatin1String("7001"));
    portEdit->setFixedWidth(75);
    layout->addWidget(new QLabel(tr("Port:")), row, 2);
    layout->addWidget(portEdit, row, 3, 1, 1, Qt::AlignRight | Qt::AlignVCenter);

    row++;
    nameEdit = new QLineEdit;
    layout->addWidget(new QLabel(tr("Name:")), row, 0);
    layout->addWidget(nameEdit, row, 1, 1, 3);

    row++;
    layout->setRowMinimumHeight(row, 16);

    row++;
    QLineEdit *usernameEdit = new QLineEdit;
    layout->addWidget(new QLabel(tr("Username:")), row, 0);
    layout->addWidget(usernameEdit, row, 1);

    QLabel *loginDefault = new QLabel(QLatin1String("<a href='default'>") + tr("Use Default")
                                      + QLatin1String("</a>"));
    loginDefault->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(loginDefault, row, 2, 1, 2, Qt::AlignRight | Qt::AlignVCenter);

    row++;
    QLineEdit *passwordEdit = new QLineEdit;
    layout->addWidget(new QLabel(tr("Password:")), row, 0);
    layout->addWidget(passwordEdit, row, 1);
    QCheckBox *passSaveChk = new QCheckBox(tr("Save password"));
    passSaveChk->setChecked(true);
    passSaveChk->setEnabled(false); // not implemented yet (#565)
    layout->addWidget(passSaveChk, row, 2, 1, 2, Qt::AlignRight | Qt::AlignVCenter);

    row++;
    QCheckBox *autoConnectChk = new QCheckBox(tr("Connect automatically at startup"));
    autoConnectChk->setChecked(true);
    layout->addWidget(autoConnectChk, row, 1, 1, 3);

    row++;
    layout->setRowMinimumHeight(row, 8);

    row++;
    QBoxLayout *loadingLayout = new QHBoxLayout;
    loadingLayout->setMargin(0);

    testResultIcon = new QLabel;
    loadingLayout->addWidget(testResultIcon);

    testResultText = new QLabel;
    testResultText->setStyleSheet(QLatin1String("font-weight:bold"));
    loadingLayout->addWidget(testResultText, 0, Qt::AlignLeft);

    layout->addLayout(loadingLayout, row, 0, 1, 4, Qt::AlignHCenter);

    registerField(QLatin1String("serverName*"), nameEdit);
    registerField(QLatin1String("serverHostname*"), hostEdit);
    registerField(QLatin1String("serverPort"), portEdit);
    registerField(QLatin1String("serverUsername*"), usernameEdit);
    registerField(QLatin1String("serverPassword*"), passwordEdit);
    registerField(QLatin1String("serverPasswordSaved"), passSaveChk);
    registerField(QLatin1String("serverAutoConnect"), autoConnectChk);

    connect(hostEdit, SIGNAL(textChanged(QString)), SLOT(hostTextChanged(QString)));
    connect(loginDefault, SIGNAL(linkActivated(QString)), SLOT(setDefaultLogin()));

    connect(hostEdit, SIGNAL(textChanged(QString)), SLOT(testLoginDelayed()));
    connect(portEdit, SIGNAL(textChanged(QString)), SLOT(testLoginDelayed()));
    connect(usernameEdit, SIGNAL(textChanged(QString)), SLOT(testLoginDelayed()));
    connect(passwordEdit, SIGNAL(textChanged(QString)), SLOT(testLoginDelayed()));
}

void SetupServerPage::initializePage()
{
    wizard()->setOptions(wizard()->options() | QWizard::HaveCustomButton1);
    connect(wizard(), SIGNAL(customButtonClicked(int)), wizard(), SLOT(skip()));
}

void SetupServerPage::save()
{
    if (!isComplete())
    {
        qDebug("Not saving new server: setup page isn't complete. Probably a bug.");
        return;
    }

    DVRServer *server = bcApp->addNewServer(field(QLatin1String("serverName")).toString());
    server->writeSetting("hostname", field(QLatin1String("serverHostname")).toString());
    server->writeSetting("port", field(QLatin1String("serverPort")).toString());
    server->writeSetting("username", field(QLatin1String("serverUsername")).toString());
    server->writeSetting("password", field(QLatin1String("serverPassword")).toString());
    server->writeSetting("autoConnect", field(QLatin1String("serverAutoConnect")).toBool());

    server->login();

    saved = true;
}

void SetupServerPage::hideEvent(QHideEvent *ev)
{
    if (!ev->spontaneous() && loginReply)
    {
        loginReply->disconnect(this);
        loginReply->abort();
        loginReply->deleteLater();
        loginReply = 0;
    }

    QWizardPage::hideEvent(ev);
}

void SetupServerPage::cleanupPage()
{
    if (loginReply)
    {
        loginReply->disconnect(this);
        loginReply->abort();
        loginReply->deleteLater();
        loginReply = 0;
    }

    if (testResultIcon->movie())
    {
        delete testResultIcon->movie();
        testResultIcon->setMovie(0);
    }
    if (testResultIcon->pixmap())
        testResultIcon->setPixmap(QPixmap());

    testResultText->clear();

    QWizardPage::cleanupPage();
    wizard()->button(QWizard::CustomButton1)->setVisible(false);
    wizard()->setOptions(wizard()->options() & ~QWizard::HaveCustomButton1);
}

void SetupServerPage::hostTextChanged(const QString &host)
{
    if (!nameEdit->isModified())
        nameEdit->setText(host);
}

void SetupServerPage::setDefaultLogin()
{
    setField(QLatin1String("serverUsername"), QLatin1String("Admin"));
    setField(QLatin1String("serverPassword"), QLatin1String("bluecherry"));
}

void SetupServerPage::testLoginDelayed()
{
    if (loginReply)
    {
        loginReply->disconnect(this);
        loginReply->abort();
        loginReply->deleteLater();
        loginReply = 0;
    }

    loginRequestTimer.start(300);
}

void SetupServerPage::testLogin()
{
    if (loginReply)
    {
        loginReply->disconnect(this);
        loginReply->abort();
        loginReply->deleteLater();
        loginReply = 0;
    }

    if (loginRequestTimer.isActive())
        loginRequestTimer.stop();

    QString hostname = field(QLatin1String("serverHostname")).toString();
    int port = field(QLatin1String("serverPort")).toInt();
    QString username = field(QLatin1String("serverUsername")).toString();
    QString password = field(QLatin1String("serverPassword")).toString();

    if (hostname.isEmpty() || port < 1 || port > 65535 || username.isEmpty() || password.isEmpty())
        return;

    /* UI */
    if (!testResultIcon->movie())
    {
        testResultIcon->setPixmap(QPixmap());
        QMovie *loadingMovie = new QMovie(QLatin1String(":/images/loading-circle.gif"), "gif", testResultIcon);
        testResultIcon->setMovie(loadingMovie);
        loadingMovie->start();
    }
    testResultText->setText(tr("Connecting..."));

    /* It'd be nice to find a better solution than (essentially) duplicating the
     * logic from ServerRequestManager, but we can't create a DVRServer yet, and
     * allowing that to operate without a DVRServer would be a non-trivial change. */
    QUrl url;
    url.setScheme(QLatin1String("https"));
    url.setHost(hostname);
    url.setPort(port);
    url.setPath(QLatin1String("/ajax/login.php"));

    QUrl queryData;
    queryData.addQueryItem(QLatin1String("login"), username);
    queryData.addQueryItem(QLatin1String("password"), password);
    queryData.addQueryItem(QLatin1String("from_client"), QLatin1String("true"));

    loginReply = bcApp->nam->post(QNetworkRequest(url), queryData.encodedQuery());
    loginReply->ignoreSslErrors();
    connect(loginReply, SIGNAL(finished()), SLOT(loginRequestFinished()));
}

void SetupServerPage::loginRequestFinished()
{
    if (!loginReply || sender() != loginReply)
        return;

    QNetworkReply *reply = loginReply;
    loginReply->deleteLater();
    loginReply = 0;

    if (testResultIcon->movie())
    {
        testResultIcon->movie()->deleteLater();
        testResultIcon->setMovie(0);
    }

    if (reply->error() != QNetworkReply::NoError)
    {
        testResultIcon->setPixmap(QPixmap(QLatin1String(":/icons/exclamation-red.png")));
        testResultText->setText(reply->errorString());
        return;
    }

    QByteArray data = reply->readAll();
    if (!data.startsWith("OK"))
    {
        testResultIcon->setPixmap(QPixmap(QLatin1String(":/icons/exclamation-red.png")));
        if (!data.isEmpty())
            testResultText->setText(QString::fromLatin1(data));
        else
            testResultText->setText(tr("Unknown login error"));
        return;
    }

    testResultIcon->setPixmap(QPixmap(QLatin1String(":/icons/tick.png")));
    testResultText->setText(tr("Login successful! Click <b>Finish</b> to continue."));
}

SetupFinishPage::SetupFinishPage()
{
    setTitle(tr("Let's Go!"));
    setSubTitle(tr("Here's some tips on how to get started:"));
    setButtonText(QWizard::FinishButton, tr("Close"));

    QBoxLayout *layout = new QVBoxLayout(this);
    layout->addStretch();
}

void SetupFinishPage::initializePage()
{
    wizard()->button(QWizard::CancelButton)->setVisible(false);
    wizard()->button(QWizard::CustomButton1)->setVisible(false);
}

void SetupFinishPage::cleanupPage()
{
    wizard()->button(QWizard::CancelButton)->setVisible(true);
}