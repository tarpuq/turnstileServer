#ifndef APP_H
#define APP_H

#include <QObject>

#include "mysql.h"
#include "gtcpserver.h"

class App : public QObject
{
    Q_OBJECT

public:
    explicit App(QObject *parent = nullptr);

    typedef struct _PlantaAccesos {
        qint32 id;
        qint8 testsRequired;
        qint8 esdTimeout;
        qint8 passTimeout;
    } plantaAccesos_t;

    void SetDataBase(QString tipo, QString host, QString dataBase,
                     QString user, QString pswd);
    void SetPort(quint16 port);

    bool getCardState(quint32 cardNumber);
    bool getAccessState(quint32 cardNumber);
    void getProfileData(quint32 cardNumber, plantaAccesos_t *info);

private slots:
    void ProcesaEstadoBD(bool stt);
    void ProcessFrame(QTcpSocket *socket, QByteArray leido);

private:
    void GeneraReporte();
    mysql *sqlX;
    GTcpServer *tcp;
    QDate dia;
    bool bdCntd;
};

#endif // APP_H
