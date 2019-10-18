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

    QString queryStr;

    bool isValidCard = false;
    queryStr = QString("SELECT estado FROM bd_plantapruebas.tarjetas_acceso WHERE codigo = %1")
                   .arg(cardNumber);

    if (query.exec(queryStr)) {
        if (query.next()) {
            isValidCard = query.value("estado").toInt();
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
            if (query.exec(queryStr)) {
                if (query.next()) {
                    isValidCard = query.value("estado").toInt();
                }
            } else {
                //  Process error
                qDebug() << "DB Error, check state";
            }
        }
    }
    return isValidCard;
}

void App::getCardData(quint32 cardNumber, plantaAccesos_t *info)
{
    QSqlDatabase db = QSqlDatabase::database("dbConexion");
    QSqlQuery query(db);

    QString queryStr;

    QString testsRequiredString = "";
    bool dataOk = false;

    queryStr = QString("SELECT id, pruebas_requeridas, tiempo_duracion FROM "
                       "bd_plantapruebas.planta_accesos WHERE tarjetas_acceso_codigo = %1")
                   .arg(cardNumber);

    if (query.exec(queryStr)) {
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
            if (query.exec(queryStr)) {
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
        testsRequiredString = query.value("pruebas_requeridas").toString();
        info->testTimeout = static_cast<qint8>(query.value("tiempo_duracion").toInt());

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

    QString testsRequiredString = "";

    plantaAccesos_t workerInfo;

    QSqlDatabase db = QSqlDatabase::database("dbConexion");
    QSqlQuery query(db);

    direction = static_cast<uint8_t>(leido.at(0)) & 0x80;
    deviceID = static_cast<uint8_t>(leido.at(0)) & 0x7F;

    messageType = static_cast<messageType_t>(leido.at(1)) ;

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

        answer.append(static_cast<char>(validCard));

        getCardData(cardNumber, &workerInfo);

        if (validCard == 1) {
            answer.append(static_cast<char>(workerInfo.testsRequired));
            answer.append(static_cast<char>(workerInfo.testTimeout));
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

        getCardData(cardNumber, &workerInfo);

        testOk = static_cast<uint32_t>(leido.at(5)) & 0x000000FF;
        esd_reporte_tipo = direction >> 7;
        filters = QString("INSERT INTO bd_plantapruebas.esd_reporte (resultado, tipo, "
                          "planta_accesos_id)"
                          "VALUES (%1,%2,%3)")
                      .arg(testOk)
                      .arg(esd_reporte_tipo)
                      .arg(workerInfo.id);

        qDebug() << filters;

        if (query.exec(filters)) {
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

        getCardData(cardNumber, &workerInfo);

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

        getCardData(cardNumber, &workerInfo);

        testOk = static_cast<uint32_t>(leido.at(5)) & 0x000000FF;
        esd_reporte_tipo = direction >> 7;

        filters = QString("INSERT INTO bd_plantapruebas.esd_reporte (resultado, tipo, "
                          "planta_accesos_id)"
                          "VALUES (%1,%2,%3)")
                      .arg(testOk)
                      .arg(esd_reporte_tipo)
                      .arg(workerInfo.id);

        qDebug() << filters;

        if (query.exec(filters)) {
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
