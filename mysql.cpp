#include "mysql.h"

mysql::mysql(QObject *parent) : QObject(parent)
{
    tmrCnx = new QTimer;
    connect(tmrCnx, SIGNAL(timeout()), this, SLOT(LeeEstado()));
    sttBd = false;
}

void mysql::InicializaDB(QString tipo, QString host, QString dataBase,
                         QString user, QString pswd)
{
    QStringList tipoList;
    QString connectString;
    tipoList = tipo.split("/");
    db = QSqlDatabase::addDatabase(tipoList.at(0), "dbConexion");
    if(tipoList.at(0).contains("QMYSQL"))
    {
        db.setHostName(host);
        db.setDatabaseName(dataBase);
        db.setUserName(user);
        db.setPassword(pswd);
    }
    else if(tipoList.at(0).contains("QODBC"))
    {
        connectString = "DRIVER={" + tipoList.at(1) + "};";
        connectString.append("SERVER=" + host + ";"); //Hostname, Instance
        connectString.append("DATABASE=" + dataBase + ";"); //Schema
        connectString.append("UID=" + user + ";"); //User
        connectString.append("PWD=" + pswd + ";"); //Pass
        db.setDatabaseName(connectString);
    }
}

void mysql::Conecta()
{
    tmrCnx->start(30000);
    db.open();
    LeeEstado();
}

void mysql::LeeEstado()
{
    bool stt;
    tmrCnx->start(30000);
    if (db.isOpen())
        stt = true;
    else
    {
        if(db.open())
            stt = true;
        else
            stt = false;
    }
    if(!stt)
        qDebug() << db.lastError();
    if(stt != sttBd)
    {
        sttBd = stt;
        emit Conexion(sttBd);
    }
}

QString mysql::UltimoError()
{
    return ultError;
}

bool mysql::ExecQuery(QString queryE)
{
    QSqlQuery query(db);
    if (db.isOpen())
    {
        if(query.exec(queryE))
            return true;
        else
            ultError = query.lastError().text();
    }
    return false;
}

bool mysql::EjecutaQuery(QString query)
{
    if(ExecQuery(query))
        return true;
    else
    {
        if (!db.isOpen())
            db.open();
        else
        {
            db.close();
            db.open();
        }
        if (db.isOpen())
        {
            if(ExecQuery(query))
                return true;
        }
    }
    return false;
}

QSqlQueryModel* mysql::ConsultaDB(QString consulta)
{
    QSqlQueryModel *model = new QSqlQueryModel();
    QSqlQuery query(consulta, db);
    if(db.isOpen())
        model->setQuery(query);
    else
    {
        db.open();
        if(db.isOpen())
            model->setQuery(query);
    }
    return model;
}
