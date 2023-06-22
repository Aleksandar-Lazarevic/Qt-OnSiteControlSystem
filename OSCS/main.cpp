#include <QApplication>
#include <QtGui/QGuiApplication>
#include <QtQuick/QQuickView>
#include <QtQml/QQmlEngine>
#include <QtQml/QQmlContext>
#include <QDebug>
#include <QtCore/QLoggingCategory>
#include <QStringList>
#include <QScreen>
#include <QtCore/QSettings>
#include <QtNetwork>
#include <QtNetwork/QNetworkConfigurationManager>
#include <QtNetwork/QNetworkSession>

#include <QLocale>
#include <QTranslator>
//#include <unistd.h>
#include <signal.h>
#include <controlleradapter.h>
#include "define.h"

#include <VLCQtCore/Common.h>
#include <VLCQtQml/QmlVideoPlayer.h>
#include <VLCQtQml/Qml.h>

#include "server/webserver.h"
#include "server/tcpserver.h"
#include "robotsmanager.h"
#include "./cameraSDK/cameramanager.h"
#include "main.h"

uint g_nTcpServerProt = 50001;
uint g_nWebServerProt = 4000;
QString g_VideoUrl[16] = {0,};
QQuickView * viewLeft;
QQuickView * viewRight;
QQuickView * viewCenter;
QList s_aryScreens = QGuiApplication::screens();

void sigHandler(int signal){
    ::signal(signal, SIG_DFL);
    qCritical() << "on-site control system exit due to SIGNAL " << signal;
    qApp->quit();
}
QQuickView* FindViewOnScreen(QScreen* pScreen, QQuickView* pExcept)
{
    if(viewLeft->screen() == pScreen && pExcept != viewLeft) return viewLeft;
    if(viewRight->screen() == pScreen && pExcept != viewRight) return viewRight;
    if(viewCenter->screen() == pScreen && pExcept != viewCenter) return viewCenter;
    return NULL;
}
QScreen* FindEmptyScreen()
{
    for(int i = 0 ; i < s_aryScreens.count() ; i++)
    {
        QScreen* pScreen = s_aryScreens[i];
        if(FindViewOnScreen(pScreen, NULL) == NULL) return pScreen;
    }
    return NULL;
}
QScreen* FindOtherScreen(QScreen* nowScreen)
{
    for(int i = 0 ; i < s_aryScreens.count() ; i++)
    {
        QScreen* pScreen = s_aryScreens[i];
        if(nowScreen != NULL && nowScreen != pScreen) return pScreen;
    }
    return NULL;
}

bool MyEventFilterObject::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::NonClientAreaMouseButtonRelease) {
        QQuickView* wnd = qobject_cast<QQuickView*>(obj);
        if(wnd)
        {
            wnd->showMaximized();
            QScreen* curScreen = wnd->screen();
            QQuickView* pOther = FindViewOnScreen(curScreen, wnd);
            if(pOther)
            {
                QScreen* pOtherScreen = FindEmptyScreen();
                if(pOtherScreen)
                {
                    pOther->setScreen(pOtherScreen);
                    pOther->setX(pOtherScreen->geometry().x());
                    pOther->setY(pOtherScreen->geometry().y());
                    pOther->showFullScreen();
                    pOther->resize(pOtherScreen->geometry().width(), pOtherScreen->geometry().height());
                    pOther->showMaximized();
                }
                else
                {
                    pOtherScreen = FindOtherScreen(curScreen);
                    if(pOtherScreen)
                    {
//                        pOther->setScreen(pOtherScreen);
//                        pOther->setX(pOtherScreen->geometry().x());
//                        pOther->setY(pOtherScreen->geometry().y());
//                        pOther->showFullScreen();
//                        pOther->resize(pOtherScreen->geometry().width(), pOtherScreen->geometry().height());
//                        pOther->showMaximized();
                    }
                }
            }


        }
        qDebug("NonClientAreaMouseButtonRelease");
        return true;
    } else {
        // standard event processing
        return QObject::eventFilter(obj, event);
    }
}


BOOL readGlobalSetting()
{
    BOOL setFlag = FALSE;

    QString filePath = QCoreApplication::applicationDirPath() + "/globalsetting.dat";
    //read file and fill tree widget
    QFile treefile(filePath);
    if (!treefile.open(QIODevice::ReadOnly))
    {
    }
    else
    {
        QByteArray treedata =treefile.readAll();
        treefile.close();

        QStringList lines;
        QString dataInString;
        dataInString = (QString)treedata;
        lines = dataInString.split(QString("\n"));
        if(lines.length() > 3)
        {
            g_nTcpServerProt = atoi(lines[1].remove("\r").toLatin1().data());
            g_nWebServerProt = atoi(lines[3].remove("\r").toLatin1().data());
            setFlag = TRUE;
        }
        if(lines.length() >= 6)
        {
            QString title = lines[4].remove("\r");
            if(title == "VideoURLList")
            {
                int lastLine = lines.length();
                if(lastLine > 5 + 16)  lastLine = 5 + 16;
                for(int index = 5; index <= lastLine; index++)
                {
                    QString urlline = lines[index].remove("\r");
                    if(urlline.indexOf("URL") >=0 && urlline.indexOf("::") >=0)
                    {
                        g_VideoUrl[index - 5] = urlline.mid(urlline.indexOf("::") + 2);
                    }

                }
            }
        }
   }

    if(setFlag == FALSE){
    }
    return setFlag;
}

void InitNetwork(QGuiApplication* app)
{
    QNetworkConfigurationManager manager;
    if (manager.capabilities() & QNetworkConfigurationManager::NetworkSessionRequired) {
        // Get saved network configuration
        QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
        settings.beginGroup(QLatin1String("QtNetwork"));
        const QString id = settings.value(QLatin1String("DefaultNetworkConfiguration")).toString();
        settings.endGroup();

        // If the saved network configuration is not currently discovered use the system default
        QNetworkConfiguration config = manager.configurationFromIdentifier(id);
        if ((config.state() & QNetworkConfiguration::Discovered) !=
            QNetworkConfiguration::Discovered) {
            config = manager.defaultConfiguration();
        }

        QNetworkSession *networkSession = new QNetworkSession(config, app);
        networkSession->open();
        networkSession->waitForOpened();

        if (networkSession->isOpen()) {
            // Save the used configuration
            QNetworkConfiguration config = networkSession->configuration();
            QString id;
            if (config.type() == QNetworkConfiguration::UserChoice) {
                id = networkSession->sessionProperty(
                        QLatin1String("UserChoiceConfiguration")).toString();
            } else {
                id = config.identifier();
            }

            QSettings settings(QSettings::UserScope, QLatin1String("QtProject"));
            settings.beginGroup(QLatin1String("QtNetwork"));
            settings.setValue(QLatin1String("DefaultNetworkConfiguration"), id);
            settings.endGroup();
        }
    }
}


int main(int argc, char *argv[])
{
    //QLoggingCategory::setFilterRules(QStringLiteral("qt.bluetooth* = true"));
//    QGuiApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
//    QGuiApplication application(argc, argv);
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication application(argc, argv);

    s_aryScreens =  QGuiApplication::screens();

    signal(SIGINT, sigHandler);
    signal(SIGSEGV, sigHandler);
    signal(SIGTERM, sigHandler);

    QStringList args = VlcCommon::args();
    QString strOption = "--intf=dummy --no-media-library --no-stats --no-osd --no-loop --no-video-title-show --drop-late-frames --network-caching=1";
    qputenv("VLC_ARGS", strOption.toUtf8());
    args = VlcCommon::args();


    VlcCommon::setPluginPath(application.applicationDirPath() + "/plugins");
    //VlcQmlVideoPlayer::registerPlugin();
    VlcQml::registerTypes();


    InitNetwork(&application);

    readGlobalSetting();

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "OSCS_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            application.installTranslator(&translator);
            break;
        }
    }
    //QRect geometry = application.primaryScreen()->geometry();

    ControllerAdapter controllerAdapter;
    RobotsManager robotsMgr;
    CameraManager cameraMgr;

    controllerAdapter.SetVideoUrls(g_VideoUrl);

    qRegisterMetaType<_stClientData>();

    QSize rc(800, 600);
    //left screen handler
//    QQuickView viewLeft;
    viewLeft = new QQuickView();
    viewLeft->rootContext()->setContextProperty("controllerAdapter", &controllerAdapter);
    viewLeft->rootContext()->setContextProperty("robotsManager", &robotsMgr);
    viewLeft->rootContext()->setContextProperty("cameraManager", &cameraMgr);
    viewLeft->setSource(QUrl(QLatin1String("qrc:/Resources/qml/leftScreenMain.qml")));
    viewLeft->setResizeMode(QQuickView::SizeRootObjectToView);
    QObject::connect(viewLeft->engine(), SIGNAL(quit()), qApp, SLOT(quit()));
    viewLeft->setTitle("OSCS Left");
    viewLeft->setMinimumSize(rc);
    viewLeft->showMaximized();


    //right screen handler
//    QQuickView viewRight;
    viewRight = new QQuickView();
    viewRight->rootContext()->setContextProperty("controllerAdapter", &controllerAdapter);
    viewRight->rootContext()->setContextProperty("robotsManager", &robotsMgr);
    viewRight->rootContext()->setContextProperty("cameraManager", &cameraMgr);
    viewRight->setSource(QUrl(QLatin1String("qrc:/Resources/qml/rightScreenMain.qml")));
    viewRight->setResizeMode(QQuickView::SizeRootObjectToView);
    QObject::connect(viewRight->engine(), SIGNAL(quit()), qApp, SLOT(quit()));
    viewRight->setTitle("OSCS Right");
    viewRight->setMinimumSize(rc);
    viewRight->showMaximized();

//    QQuickView viewCenter;
    viewCenter = new QQuickView();
    viewCenter->rootContext()->setContextProperty("controllerAdapter", &controllerAdapter);
    viewCenter->rootContext()->setContextProperty("robotsManager", &robotsMgr);
    viewCenter->rootContext()->setContextProperty("cameraManager", &cameraMgr);
    viewCenter->setSource(QUrl(QLatin1String("qrc:/Resources/qml/main.qml")));
    viewCenter->setResizeMode(QQuickView::SizeRootObjectToView);
    QObject::connect(viewCenter->engine(), SIGNAL(quit()), qApp, SLOT(quit()));
    viewCenter->setTitle("OSCS Center");
    viewCenter->setMinimumSize(rc);
    viewCenter->showMaximized();    

    WebServer dataServer(g_nWebServerProt);
    QObject::connect(&dataServer, &WebServer::signal_RecvWSNewMessage, &robotsMgr, &RobotsManager::onReceiveRobotDeviceWSData);
    QObject::connect(&dataServer, &WebServer::signal_DisconnectedDevice, &robotsMgr, &RobotsManager::onRobotDeviceWSDisconnected);

    TcpServer tcpserver(g_nTcpServerProt, true);
    QObject::connect(&tcpserver, &TcpServer::siganl_tcpReceiveData, &robotsMgr, &RobotsManager::onReceiveRobotDeviceCSData);
    QObject::connect(&tcpserver, &TcpServer::siganl_tcpClientConnect, &robotsMgr, &RobotsManager::onTcpClientConnect);
    QObject::connect(&tcpserver, &TcpServer::siganl_tcpClientDisconnect, &robotsMgr, &RobotsManager::onTcpClientDisconnect);

    MyEventFilterObject *eventFilter = new MyEventFilterObject();
    viewCenter->installEventFilter(eventFilter);
    viewLeft->installEventFilter(eventFilter);
    viewRight->installEventFilter(eventFilter);

    return application.exec();
}

ScreenManager::ScreenManager()
{

}

void ScreenManager::initScreenTimer(int interval)
{

}

ScreenManager::~ScreenManager()
{

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////

QString g_token = "";
QString g_userID = "";
QString g_userPWD = "";

bool requestLogin(QString id, QString pwd, QString & errMsg)
{
    bool loginOK = false;
    errMsg = "";

    QNetworkAccessManager *mgr = new QNetworkAccessManager(NULL);
    const QUrl url(QStringLiteral("http://211.159.167.186:4000/api/users/authenticate"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject obj;
    obj["account_id"] = id;
    obj["password"] = pwd;
    obj["slider_value"] = true;
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();
    // or
    // QByteArray data("{\"key1\":\"value1\",\"key2\":\"value2\"}");
    QNetworkReply *reply = mgr->post(request, data);

    QEventLoop eventLoop;

    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));
    timer.start(5000);

    QObject::connect(mgr, SIGNAL(finished(QNetworkReply*)), &eventLoop, SLOT(quit()));
    eventLoop.exec();

    if(timer.isActive()) {
        timer.stop();
        if(reply->error() == QNetworkReply::NoError)
        {
            QString contents = QString::fromUtf8(reply->readAll());
            qDebug() << contents;

            QJsonObject jsonObj;
            QJsonDocument jsonDoc = QJsonDocument::fromJson(contents.toUtf8());

            // check validity of the document
            if(!jsonDoc.isNull())
            {
                if(jsonDoc.isObject())
                {
                    jsonObj = jsonDoc.object();

                    foreach(const QString& key, jsonObj.keys())
                    {
                        if(key == "token")
                        {
                            g_token = jsonObj.value(key).toString();
                            g_userID = id;
                            g_userPWD = pwd;
                            qDebug() << "Key = " << key << ", Value = " << g_token;
                            errMsg =  "Login_OK:" + g_token;
                            loginOK = true;
                            break;
                        }
                    }
                }
                else
                {
                    qDebug() << "Document is not an object" << endl;
                    errMsg = "Document is not an object";
                }
            }
            else
            {
               qDebug() << "Invalid JSON...\n" ;
               errMsg = "Invalid JSON...\n" ;
            }
        }
        else{
            errMsg = reply->errorString();
        }
    }
    else
    {
       // timeout
       QObject::disconnect(reply, SIGNAL(finished()), &eventLoop, SLOT(quit()));
       reply->abort();
       errMsg = "Connection Timeout!";

    }

    reply->deleteLater();
    qDebug() << errMsg;

    return loginOK;
}

void requestLogoff()
{
    QNetworkAccessManager *mgr = new QNetworkAccessManager(NULL);
    const QUrl url(QStringLiteral("http://211.159.167.186:4000/api/users/logout"));
    QNetworkRequest request(url);

    QString JWT = g_token;
    QString headerData = "Bearer " + JWT;
    request.setRawHeader("Authorization", headerData.toLocal8Bit());


    QJsonObject obj;
    obj["account_ID"] = "12345678";
    QJsonDocument doc(obj);
    QByteArray data = doc.toJson();
    // or
    // QByteArray data("{\"key1\":\"value1\",\"key2\":\"value2\"}");
    QNetworkReply *reply = mgr->post(request, data);

    QObject::connect(reply, &QNetworkReply::finished, [=](){
        if(reply->error() == QNetworkReply::NoError){
            QString contents = QString::fromUtf8(reply->readAll());
            qDebug() << contents;
        }
        else{
            QString err = reply->errorString();
            qDebug() << err;
        }
        reply->deleteLater();
    });
}


