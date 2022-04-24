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

    ///Get the access token
    /// \brief getAccesToken
    ///redirect_uri=https://www.pushbullet.com/login-success
    ///Copy the Access token from redirect url and paste it inside token dialog
    /// e.g: https://www.pushbullet.com/login-success#access_token=o.0AGv7V5h1yrVo8p5yC3GdkBeOfY
    /// \return
    ///
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
