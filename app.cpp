#include "app.h"

#include <QDebug>

typedef enum _MessageType {
    MSG_TIMESTAMP = 0,
    MSG_ENTRY_REQUIRED,
    MSG_ENTRY_RESULT,
    MSG_EXIT_REQUIRED,
    MSG_EXIT_RESULT
} messageType_t;

#define TEST_TALONERA 0x01
#define TEST_MANILLA 0x02

App::App(QObject *parent)
    : QObject(parent)
{
    sqlX = new mysql;
    connect(sqlX, SIGNAL(Conexion(bool)), this, SLOT(ProcesaEstadoBD(bool)));
    tcp = new GTcpServer;
    connect(tcp, &GTcpServer::readData, this, &App::ProcessFrame);
    dia = QDate::currentDate();
}

void App::ProcesaEstadoBD(bool stt)
{
    qDebug() << "Conexion base de datos:" << (stt ? "Conectado" : "Desconectado");
    bdCntd = stt;
}

void App::SetDataBase(QString tipo, QString host, QString dataBase, QString user, QString pswd)
{
    qDebug() << "Iniciando..";
    sqlX->InicializaDB(tipo, host, dataBase, user, pswd);
    sqlX->Conecta();
}

void App::SetPort(quint16 port)
{
    qDebug() << "Escuchando:" << port;
    tcp->setPort(port);
    tcp->performListening();
    //repor->GuardarX();
}

bool App::getCardState(quint32 cardNumber)
{
    QSqlDatabase db = QSqlDatabase::database("dbConexion");
    QSqlQuery query(db);

    bool res = false;
    query.prepare("SELECT estado FROM tarjetas_acceso WHERE codigo = :code");
    query.bindValue(":code", cardNumber);

    if (query.exec()) {
        if (query.next()) {
            res = query.value("estado").toInt();
        }
    } else {
        //  Process error
        qDebug() << "DB Error, check state";
        if (!db.isOpen()) {
            db.open();
        } else {
            db.close();
            db.open();
        }
        if (db.isOpen()) {
            if (query.exec()) {
                if (query.next()) {
                    res = query.value("estado").toInt();
                }
            } else {
                //  Process error
                qDebug() << "DB Error, check state";
            }
        }
    }
    return res;
}

bool App::getAccessState(quint32 cardNumber)
{
    QSqlDatabase db = QSqlDatabase::database("dbConexion");
    QSqlQuery query(db);

    bool res = false;
    query.prepare(
        "SELECT estado FROM planta_accesos WHERE estado = 1 AND tarjetas_acceso_codigo = :code");
    query.bindValue(":code", cardNumber);

    if (query.exec()) {
        if (query.next()) {
            res = query.value("estado").toInt();
        }
    } else {
        //  Process error
        qDebug() << "DB Error, check state";
        if (!db.isOpen()) {
            db.open();
        } else {
            db.close();
            db.open();
        }
        if (db.isOpen()) {
            if (query.exec()) {
                if (query.next()) {
                    res = query.value("estado").toInt();
                }
            } else {
                //  Process error
                qDebug() << "DB Error, check state";
            }
        }
    }
    return res;
}

void App::getProfileData(quint32 cardNumber, plantaAccesos_t *info)
{
    QSqlDatabase db = QSqlDatabase::database("dbConexion");
    QSqlQuery query(db);

    QString testsRequiredString = "";
    bool dataOk = false;

    query.prepare("SELECT planta_accesos.id,usuario_perfil.pruebas,usuario_perfil.tiempo_duracion,usuario_perfil.tiempo_esd "
                  "FROM bd_plantapruebas.planta_accesos INNER JOIN bd_plantapruebas.usuario_perfil "
                  "ON planta_accesos.usuario_perfil_id = usuario_perfil.id "
                  "WHERE tarjetas_acceso_codigo = :code AND estado = 1;");
    query.bindValue(":code", cardNumber);

    if (query.exec()) {
        if (query.next()) {
            dataOk = true;
        }
    } else {
        //  Process error
        qDebug() << "DB Error, get time";
        if (!db.isOpen()) {
            db.open();
        } else {
            db.close();
            db.open();
        }
        if (db.isOpen()) {
            if (query.exec()) {
                if (query.next()) {
                    dataOk = true;
                }
            } else {
                //  Process error
                qDebug() << "DB Error, get time";
            }
        }
    }

    if (dataOk) {
        info->id = query.value("id").toInt();
        info->testsRequired = 0;
        testsRequiredString = query.value("pruebas").toString();
        info->esdTimeout = static_cast<qint8>(query.value("tiempo_esd").toInt());
        info->passTimeout = static_cast<qint8>(query.value("tiempo_duracion").toInt());

        if (testsRequiredString.contains("TALONERA")) {
            info->testsRequired |= TEST_TALONERA;
        }

        if (testsRequiredString.contains("MANILLA")) {
            info->testsRequired |= TEST_MANILLA;
        }
    }
}

void App::ProcessFrame(QTcpSocket *socket, QByteArray leido)
{
    QByteArray answer;
    QString filters;
    uint8_t direction = 0;
    uint8_t deviceID = 0;
    uint8_t testOk = 0;
    uint8_t esd_reporte_tipo = 0;
    messageType_t messageType;
    uint32_t cardNumber = 0;
    int validCard = 0;
    int accessStatus = 0;

    QString testsRequiredString = "";

    plantaAccesos_t workerInfo;

    QSqlDatabase db = QSqlDatabase::database("dbConexion");
    QSqlQuery query(db);

    direction = static_cast<uint8_t>(leido.at(0)) & 0x80;
    deviceID = static_cast<uint8_t>(leido.at(0)) & 0x7F;

    messageType = static_cast<messageType_t>(leido.at(1));

    answer.clear();
    answer.append(static_cast<char>(direction | deviceID));
    answer.append(static_cast<char>(messageType));

    QString frame = QString("Dev:%1, Msg:%2").arg(deviceID).arg(messageType);
    qDebug() << frame;

    QDateTime timeStamp = QDateTime::currentDateTime();

    switch (messageType) {
    case MSG_TIMESTAMP:
        answer.append(static_cast<char>(timeStamp.date().day()));
        answer.append(static_cast<char>(timeStamp.date().month()));
        answer.append(static_cast<char>(timeStamp.date().year() - 2000));
        answer.append(static_cast<char>(timeStamp.time().hour()));
        answer.append(static_cast<char>(timeStamp.time().minute()));
        answer.append(static_cast<char>(timeStamp.time().second()));
        break;
    case MSG_ENTRY_REQUIRED:
        cardNumber = static_cast<uint32_t>(leido.at(2)) & 0x000000FF;
        cardNumber |= (static_cast<uint32_t>(leido.at(3)) << 8) & 0x0000FF00;
        cardNumber |= (static_cast<uint32_t>(leido.at(4)) << 16) & 0x00FF0000;

        validCard = getCardState(cardNumber);

        if (validCard == 1) {
            accessStatus = getAccessState(cardNumber);

            getProfileData(cardNumber, &workerInfo);

            answer.append(static_cast<char>(accessStatus));

            if (accessStatus == 1){
                answer.append(static_cast<char>(workerInfo.testsRequired));
                answer.append(static_cast<char>(workerInfo.esdTimeout));
                answer.append(static_cast<char>(workerInfo.passTimeout));
            } else {
                //  An invalid asigned card is being used
                qDebug() << "Invalid asigned ID card";
            }
        } else {
            //  An invalid card is being used
            qDebug() << "Invalid ID card";
        }
        break;
    case MSG_ENTRY_RESULT:
        cardNumber = static_cast<uint32_t>(leido.at(2)) & 0x000000FF;
        cardNumber |= (static_cast<uint32_t>(leido.at(3)) << 8) & 0x0000FF00;
        cardNumber |= (static_cast<uint32_t>(leido.at(4)) << 16) & 0x00FF0000;

        validCard = getCardState(cardNumber);

        answer.append(static_cast<char>(validCard));

        getProfileData(cardNumber, &workerInfo);

        testOk = static_cast<uint32_t>(leido.at(5)) & 0x000000FF;
        esd_reporte_tipo = direction >> 7;

        query.prepare("INSERT INTO bd_plantapruebas.esd_reporte (resultado, tipo, "
                      "planta_accesos_id)"
                      "VALUES (:test, :esd_reporte_tipo, :user_id);");
        query.bindValue(":test", testOk);
        query.bindValue(":esd_reporte_tipo", esd_reporte_tipo);
        query.bindValue(":user_id", workerInfo.id);

        if (query.exec()) {
            //  Process ok
            qDebug() << "DB OK";
        } else {
            //  Process error
            qDebug() << "DB ERROR";
        }

        break;
    case MSG_EXIT_REQUIRED:
        cardNumber = static_cast<uint32_t>(leido.at(2)) & 0x000000FF;
        cardNumber |= (static_cast<uint32_t>(leido.at(3)) << 8) & 0x0000FF00;
        cardNumber |= (static_cast<uint32_t>(leido.at(4)) << 16) & 0x00FF0000;

        validCard = getCardState(cardNumber);

        answer.append(static_cast<char>(validCard));

        getProfileData(cardNumber, &workerInfo);

        if (validCard == 1) {
            //  A valid card is being used
        } else {
            //  An invalid card is being used
            qDebug() << "Invalid ID card";
        }
        break;
    case MSG_EXIT_RESULT:
        cardNumber = static_cast<uint32_t>(leido.at(2)) & 0x000000FF;
        cardNumber |= (static_cast<uint32_t>(leido.at(3)) << 8) & 0x0000FF00;
        cardNumber |= (static_cast<uint32_t>(leido.at(4)) << 16) & 0x00FF0000;

        validCard = getCardState(cardNumber);

        answer.append(static_cast<char>(validCard));

        getProfileData(cardNumber, &workerInfo);

        testOk = static_cast<uint32_t>(leido.at(5)) & 0x000000FF;
        esd_reporte_tipo = direction >> 7;

        query.prepare("INSERT INTO bd_plantapruebas.esd_reporte (resultado, tipo, "
                      "planta_accesos_id)"
                      "VALUES (:test, :esd_reporte_tipo, :user_id);");
        query.bindValue(":test", testOk);
        query.bindValue(":esd_reporte_tipo", esd_reporte_tipo);
        query.bindValue(":user_id", workerInfo.id);

        if (query.exec()) {
            //  Process ok
            qDebug() << "DB OK";
        } else {
            //  Process error
            qDebug() << "DB ERROR";
        }
        break;
    }

    tcp->writeData(socket, answer);
}
