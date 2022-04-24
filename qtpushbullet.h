#ifndef QTPUSHBULLET_H
#define QTPUSHBULLET_H

#include <QObject>
#include <QJsonDocument>
#include <QJsonArray>
#include <QApplication>
#include <QWebSocket>

class QJsonObject;

class QtPushBullet : public QObject
{
    Q_OBJECT
public:
    explicit QtPushBullet(const QString &clientID,
                          const QString &clientSecret,
                          QObject *parent = nullptr);

    void pushNote(const QString &title, const QString &body);
    void pushLink(const QString &title, const QString &body, const QString &url);
    void pushFile(const QString &fileName, const QString &body);
    void sendSMS(const QString &targetDeviceIden,
                 const QStringList &addresses,
                 const QString &msg,
                 const std::function<void(const QByteArray &data)> &callback = nullptr);

    void me();
    void sendEphemeral(const QVariantHash &data);
    void listDevices();

    //List sms enabled devices
    QMap<QString, QString> getSMSDevices();

    //Realtime Event Stream
    QWebSocket *startRealtimeStream();

signals:
    void authorized();
public slots:

private:
    QString m_clientID;
    QString m_clientSecret;
    QString m_authToken;
    QString m_iden;
    QString m_name;
    QJsonArray m_devices;
    QWebSocket *m_webSocket;

    //Authorization
    bool getAccesToken();
    void saveAuthToken(const QJsonDocument &authToken);
    void readAuthToken();

    //
    void getApi(const QUrl &url,
                const std::function<void(const QByteArray &data)> &callback = nullptr);

    void postApi(const QUrl &url, QJsonDocument json,
                 const std::function<void(const QByteArray &data)> &callback = nullptr);

    void uploadFile(const QString &uploadUrl,
                    const QString &fileName,
                    const std::function<void(const QByteArray &data)> &callback = nullptr);
};

#endif // QTPUSHBULLET_H
