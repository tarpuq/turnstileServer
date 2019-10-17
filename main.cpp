#include <QCoreApplication>
#include <QtGlobal>
#include "app.h"

#include <iostream>
#include <signal.h>

static QCoreApplication *app;

#ifdef Q_OS_WIN
typedef void (*SignalHandlerPointer)(int);
#elif defined (Q_OS_LINUX)
/* signal handling variables */
struct sigaction sigact; /* SIGQUIT&SIGINT&SIGTERM signal handling */
static int exit_sig = 0; /* 1 -> application terminates cleanly (shut down hardware, close open files, etc) */
static int quit_sig = 0; /* 1 -> application terminates without shutting down the hardware */
#endif

static void signal_handler(int sig)
{
    std::cout << "received signal " << sig << std::endl;

    switch(sig) {
    case SIGINT:
        app->quit();
        break;
    case SIGTERM:
        app->quit();
        break;

    }
}

/** Search the configuration file */
QString searchConfigFile()
{
    QString binDir=QCoreApplication::applicationDirPath();
    QString appName=QCoreApplication::applicationName();
    QString fileName(appName+".conf");

    QStringList searchList;
    searchList.append(binDir);
    searchList.append(binDir+"/etc");
    searchList.append(binDir+"/../etc");
    searchList.append(binDir+"/../../etc"); // for development without shadow build
    searchList.append(binDir+"/../"+appName+"/etc"); // for development with shadow build
    searchList.append(binDir+"/../../"+appName+"/etc"); // for development with shadow build
    searchList.append(binDir+"/../../../"+appName+"/etc"); // for development with shadow build
    searchList.append(binDir+"/../../../../"+appName+"/etc"); // for development with shadow build
    searchList.append(binDir+"/../../../../../"+appName+"/etc"); // for development with shadow build
    searchList.append(QDir::rootPath()+"usr/local/etc");
    searchList.append(QDir::rootPath()+"etc/opt");
    searchList.append(QDir::rootPath()+"etc");

    foreach (QString dir, searchList)
    {
        QFile file(dir+"/"+fileName);
        if (file.exists())
        {
            // found
            fileName=QDir(file.fileName()).canonicalPath();
            qDebug("Using config file %s",qPrintable(fileName));
            return fileName;
        }
    }

    // not found
    foreach (QString dir, searchList)
    {
        qWarning("%s/%s not found",qPrintable(dir),qPrintable(fileName));
    }
    qFatal("Cannot find config file %s",qPrintable(fileName));
    return nullptr;
}

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);
    App *appQ = new App;
    QStringList arch;

    QString configFileName = searchConfigFile();
    QSettings* settings=new QSettings(configFileName,QSettings::IniFormat,&a);

    //  Variables
    QString dbHost;
    QString dbName;
    QString dbUser;
    QString dbPass;
    quint16 server_port;

    //  Database
    settings->beginGroup("database");
    dbHost = settings->value("dbHost").toString();
    dbName = settings->value("dbName").toString();
    dbUser = settings->value("dbUser").toString();
    dbPass = settings->value("dbPass").toString();
    settings->endGroup();

    //  Servidor
    settings->beginGroup("server");
    server_port = static_cast<quint16>(settings->value("port").toInt());
    settings->endGroup();

    appQ->SetDataBase("QMYSQL", dbHost, dbName, dbUser, dbPass);
    appQ->SetPort(server_port);

    /* configure signal handling */
#ifdef Q_OS_WIN
    SignalHandlerPointer previousHandler;
    previousHandler = signal(SIGABRT, signal_handler);
#elif defined (Q_OS_LINUX)
    sigemptyset(&sigact.sa_mask);
    sigact.sa_flags = 0;
    sigact.sa_handler = signal_handler;
    sigaction(SIGINT, &sigact, NULL);
    sigaction(SIGTERM, &sigact, NULL);
#endif

    app = &a;

    return a.exec();
}
