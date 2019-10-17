#ifndef MYSQL_H
#define MYSQL_H

#include <QObject>
#include <QtSql>

class mysql : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief mysql Inicializa el objeto
     * @param parent Parent del objeto
     */
    explicit mysql(QObject *parent = nullptr);
    /**
     * @brief InicializaDB Setea los datos de la conexion de base de datos
     * @param tipo Tipo de conexion (Ej: MYSQL)
     * @param host Direccion de la base de datos
     * @param dataBase Nombre de la base de datos
     * @param user Usuario de la base de datos
     * @param pswd Contrasena de la base de datos
     */
    void InicializaDB(QString tipo, QString host, QString dataBase,
                      QString user, QString pswd);
    /**
     * @brief Conecta Conecta a la base de datos, con los datos seteados en Inicializa DB
     */
    void Conecta();
    /**
     * @brief UltimoError
     * @return Devuelve el ultimo error al subir datos
     */
    QString UltimoError();
    /**
     * @brief EjecutaQuery Ejecuta un query, verifica conexion
     * @param queryE String de la consulta
     * @return True si se realiza correctamente
     */
    bool EjecutaQuery(QString queryE);
    /**
     * @brief ConsultaDB Realiza una consulta a la base de datos
     * @param consulta Consulta a realizar (query)
     * @return Un modelo de tabla resultante de la consulta
     */
    QSqlQueryModel* ConsultaDB(QString consulta);

signals:
    void Conexion(bool stt);

private slots:
    /**
     * @brief LeeEstado
     */
    void LeeEstado();

private:
    QSqlDatabase db;
    QTimer *tmrCnx;
    QString ultError;
    bool sttBd;
    /**
     * @brief ExecQuery Ejecuta un query comprobando que la base de datos este conectada
     * @param queryE Consulta a realizar (envio de datos)
     * @return True si se ejecuto correctamente
     */
    bool ExecQuery(QString queryE);
};

#endif // MYSQL_H
