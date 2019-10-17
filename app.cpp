#include "app.h"

#include <QDebug>

enum _MessageType
{
    MSG_ENTRY_REQUIRED = 0,
    MSG_ENTRY_RESULT,
    MSG_EXIT_REQUIRED,
    MSG_EXIT_RESULT
};

#define TEST_TALONERA   0x01
#define TEST_MANILLA    0x02

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

void App::ProcessFrame(QTcpSocket *socket, QByteArray leido)
{
    QByteArray answer;
    QString filters;
    uint8_t direction = 0;
    uint8_t deviceID = 0;
    uint8_t testOk = 0;
    uint8_t esd_reporte_tipo = 0;
    uint8_t messageType = 0;
    uint32_t cardNumber = 0;
    int cardStatus = 0;

    QString testsRequiredString = "";
    uint8_t testsRequired = 0;
    int planta_accesos_id = 0;
    int testsTimeout = 0;

    QSqlDatabase db = QSqlDatabase::database("dbConexion");
    QSqlQuery query(db);

    direction = static_cast<uint8_t>(leido.at(0)) & 0x80;
    deviceID = static_cast<uint8_t>(leido.at(0)) & 0x7F;

    messageType = static_cast<uint8_t>(leido.at(1)) & 0x000000FF;

    cardNumber = static_cast<uint32_t>(leido.at(2)) & 0x000000FF;
    cardNumber |= (static_cast<uint32_t>(leido.at(3)) << 8) & 0x0000FF00;
    cardNumber |= (static_cast<uint32_t>(leido.at(4)) << 16) & 0x00FF0000;

    QString frame
        = QString("Dev:%1, Msg:%2, CardNumber:%3").arg(deviceID).arg(messageType).arg(cardNumber);
    qDebug() << frame;

    filters = QString("SELECT estado FROM bd_plantapruebas.tarjetas_acceso WHERE codigo = %1")
                  .arg(cardNumber);

    if (query.exec(filters)) {
        if (query.next()) {
            cardStatus = query.value("estado").toInt();
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
            if (query.exec(filters)) {
                if (query.next()) {
                    cardStatus = query.value("estado").toInt();
                }
            } else {
                //  Process error
                qDebug() << "DB Error, check state";
            }
        }
    }

    answer.clear();
    answer.append(static_cast<char>(direction | deviceID));
    answer.append(static_cast<char>(messageType));
    answer.append(static_cast<char>(cardStatus));

    if (cardStatus == 1) {
        filters = QString("SELECT id, pruebas_requeridas, tiempo_duracion FROM "
                          "bd_plantapruebas.planta_accesos WHERE tarjetas_acceso_codigo = %1")
                      .arg(cardNumber);

        if (query.exec(filters)) {
            if (query.next()) {
                planta_accesos_id = query.value("id").toInt();
                testsRequiredString = query.value("pruebas_requeridas").toString();
                testsTimeout = query.value("tiempo_duracion").toInt();
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
                if (query.exec(filters)) {
                    if (query.next()) {
                        planta_accesos_id = query.value("id").toInt();
                        testsRequiredString = query.value("pruebas_requeridas").toString();
                        testsTimeout = query.value("tiempo_duracion").toInt();
                    }
                } else {
                    //  Process error
                    qDebug() << "DB Error, get time";
                }
            }
        }

        switch (messageType) {
        case MSG_ENTRY_REQUIRED:
            if (testsRequiredString.contains("TALONERA")) {
                testsRequired |= TEST_TALONERA;
            }

            if (testsRequiredString.contains("MANILLA")) {
                testsRequired |= TEST_MANILLA;
            }

            answer.append(static_cast<char>(testsRequired));
            answer.append(static_cast<char>(testsTimeout));
            break;
        case MSG_ENTRY_RESULT:
            testOk = static_cast<uint32_t>(leido.at(5)) & 0x000000FF;
            esd_reporte_tipo = direction >> 7;

            filters = QString("INSERT INTO bd_plantapruebas.esd_reporte (resultado, tipo, "
                              "planta_accesos_id)"
                              "VALUES (%1,%2,%3)")
                          .arg(testOk)
                          .arg(esd_reporte_tipo)
                          .arg(planta_accesos_id);

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
            break;
        case MSG_EXIT_RESULT:
            testOk = static_cast<uint32_t>(leido.at(5)) & 0x000000FF;
            esd_reporte_tipo = direction >> 7;

            filters = QString("INSERT INTO bd_plantapruebas.esd_reporte (resultado, tipo, "
                              "planta_accesos_id)"
                              "VALUES (%1,%2,%3)")
                          .arg(testOk)
                          .arg(esd_reporte_tipo)
                          .arg(planta_accesos_id);

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

    } else {
        //  An invalid card is being used
        qDebug() << "Invalid ID card";
    }

    tcp->writeData(socket, answer);
}
