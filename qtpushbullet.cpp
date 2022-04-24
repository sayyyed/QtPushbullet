#include "qtpushbullet.h"
#include <QFile>
#include <QJsonObject>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QMessageBox>
#include <QDesktopServices>
#include <QInputDialog>
#include <QHttpMultiPart>
#include <QMimeDatabase>
#include <QFileInfo>

QtPushBullet::QtPushBullet(const QString &clientID, const QString &clientSecret, QObject *parent) : QObject(parent)
{
    m_clientID = clientID;
    m_clientSecret = clientSecret;
    m_authToken = "";

    readAuthToken();

    connect(this, &QtPushBullet::authorized, [=]()
    {
        QJsonDocument doc;
        QJsonObject rootObject;
        rootObject.insert("access_token", m_authToken);
        rootObject.insert("iden", m_iden);
        rootObject.insert("name", m_name);
        rootObject.insert("devices", m_devices);
        doc.setObject(rootObject);

        saveAuthToken(doc);
    });
}

QWebSocket * QtPushBullet::startRealtimeStream()
{
    if(m_authToken.isEmpty())
    {
        if(!getAccesToken())
            return nullptr;
    }

    if(!m_webSocket)
        m_webSocket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    if(!m_webSocket->isValid())
        m_webSocket->open(QUrl::fromUserInput("wss://stream.pushbullet.com/websocket/"+m_authToken));

    return m_webSocket;
}

void QtPushBullet::pushNote(const QString &title, const QString &body)
{
    if(m_authToken.isEmpty())
    {
        if(!getAccesToken())
            return;
    }

    QJsonDocument doc;
    QJsonObject rootObject;

    rootObject.insert("title", title);
    rootObject.insert("body", body);
    rootObject.insert("type", "note");

    doc.setObject(rootObject);

    postApi(QUrl::fromUserInput("https://api.pushbullet.com/v2/pushes"), doc);
}

void QtPushBullet::pushLink(const QString &title,
                            const QString &body,
                            const QString &url)
{
    if(m_authToken.isEmpty())
    {
        if(!getAccesToken())
            return;
    }

    QJsonDocument doc;
    QJsonObject rootObject;

    rootObject.insert("title", title);
    rootObject.insert("body", body);
    rootObject.insert("url", url);
    rootObject.insert("type", "link");

    doc.setObject(rootObject);

    postApi(QUrl::fromUserInput("https://api.pushbullet.com/v2/pushes"), doc);
}

void QtPushBullet::pushFile(const QString &fileName, const QString &body)
{
    if(!QFile::exists(fileName))
        return;

    if(m_authToken.isEmpty())
    {
        if(!getAccesToken())
            return;
    }

    QJsonDocument doc;
    QJsonObject rootObject;

    QMimeDatabase db;
    QMimeType mime = db.mimeTypeForFile(fileName);

    rootObject.insert("file_name", QFileInfo(fileName).fileName());
    rootObject.insert("file_type", mime.name());

    doc.setObject(rootObject);

    postApi(QUrl::fromUserInput("https://api.pushbullet.com/v2/upload-request"), doc,
            [=](const QByteArray &data)
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject rootObj = doc.object();

        QString file_name = rootObj.value("file_name").toString();
        QString file_type = rootObj.value("file_type").toString();
        QString file_url = rootObj.value("file_url").toString();
        QString upload_url = rootObj.value("upload_url").toString();

        uploadFile(upload_url, fileName, [=](const QByteArray &)
        {
            QJsonDocument doc;
            QJsonObject rootObject;

            rootObject.insert("file_name", file_name);
            rootObject.insert("file_type", file_type);
            rootObject.insert("file_url", file_url);
            rootObject.insert("body", body);
            rootObject.insert("type", "file");

            doc.setObject(rootObject);

            postApi(QUrl::fromUserInput("https://api.pushbullet.com/v2/pushes"), doc);
        });
    });
}

void QtPushBullet::me()
{    
    if(m_authToken.isEmpty())
    {
        if(!getAccesToken())
            return;
    }

    getApi(QUrl::fromUserInput("https://api.pushbullet.com/v2/users/me"),
           [=](const QByteArray &data)
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject rootObj = doc.object();
        m_iden = rootObj.value("iden").toString();
        m_name = rootObj.value("name").toString();

        listDevices();
    });
}

void QtPushBullet::listDevices()
{
    if(m_authToken.isEmpty())
    {
        if(!getAccesToken())
            return;
    }

    getApi(QUrl::fromUserInput("https://api.pushbullet.com/v2/devices"),
           [=](const QByteArray &data)
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonObject rootObj = doc.object();
        m_devices = rootObj.value("devices").toArray();

        emit authorized();
    });
}

QMap<QString, QString> QtPushBullet::getSMSDevices()
{
    QMap<QString, QString> devices;
    for (int i = 0; i < m_devices.size(); ++i)
    {
        QJsonObject device = m_devices.at(i).toObject();
        if(device.value("active").toBool() && device.value("has_sms").toBool() && device.value("pushable").toBool())
            devices[device.value("nickname").toString()] = device.value("iden").toString();
    }
    return devices;
}

void QtPushBullet::sendSMS(const QString &targetDeviceIden,
                           const QStringList &addresses,
                           const QString &msg,
                           const std::function<void(const QByteArray &)> &callback)
{
    if(m_authToken.isEmpty())
    {
        if(!getAccesToken())
            return;
    }

    QJsonDocument doc;
    QJsonObject rootObject;
    QJsonObject dataObject;

    dataObject.insert("addresses", QJsonArray::fromStringList(addresses));
    dataObject.insert("message", msg);
    dataObject.insert("target_device_iden", targetDeviceIden);

    rootObject.insert("data", dataObject);
    rootObject.insert("skip_delete_file", false);

    doc.setObject(rootObject);

    postApi(QUrl::fromUserInput("https://api.pushbullet.com/v2/texts"), doc,
            [=](const QByteArray &data)
    {
        if(callback)
            callback(data);
    });
}

void QtPushBullet::sendEphemeral(const QVariantHash &data)
{
    if(m_authToken.isEmpty())
    {
        if(!getAccesToken())
            return;
    }

    QJsonDocument doc;
    QJsonObject rootObject;

    rootObject.insert("push", QJsonObject::fromVariantHash(data));
    rootObject.insert("type", "push");

    doc.setObject(rootObject);

    postApi(QUrl::fromUserInput("https://api.pushbullet.com/v2/ephemerals"), doc);
}

bool QtPushBullet::getAccesToken()
{
    QString url = "https://www.pushbullet.com/authorize?"
                  "client_id="+m_clientID+
                  "&redirect_uri=https%3A%2F%2Fwww.pushbullet.com%2Flogin-success"
                  "&response_type=token";

    QDesktopServices::openUrl(QUrl::fromUserInput(url));

    bool ok = false;
    QString token = QInputDialog::getText(nullptr, tr("Access Token"),
                                          tr("Access Token:"),
                                         QLineEdit::Normal, QString(), &ok);

    if(ok & !token.isEmpty())
    {
        m_authToken = token;
        emit authorized();
        return true;
    }

    return false;
}

void QtPushBullet::saveAuthToken(const QJsonDocument &authToken)
{
    QFile file(qApp->applicationDirPath()+"/pbdata");
    if(file.open(QIODevice::WriteOnly))
    {
        QDataStream out(&file);
        out << authToken.toJson().toBase64();
        file.close();
    }
}

void QtPushBullet::readAuthToken()
{
    QFile file(qApp->applicationDirPath()+"/pbdata");
    if(file.open(QIODevice::ReadOnly))
    {
        QByteArray json;
        QDataStream in(&file);
        in >> json;
        file.close();

        json = QByteArray::fromBase64(json);

        QJsonDocument doc = QJsonDocument::fromJson(json);
        QJsonObject rootObj = doc.object();

        m_authToken = rootObj.value("access_token").toString();
        m_iden = rootObj.value("iden").toString();
        m_name = rootObj.value("name").toString();
        m_devices = rootObj.value("devices").toArray();
    }
}

void QtPushBullet::getApi(const QUrl &url,
                          const std::function<void (const QByteArray &)> &callback)
{
    QNetworkAccessManager *m = new QNetworkAccessManager;
    QNetworkRequest request(url);
    request.setRawHeader(QByteArray("Access-Token"), m_authToken.toLatin1());

    QNetworkReply *reply = m->get(request);
    connect(reply, &QNetworkReply::finished, [=]()
    {
        if(callback)
            callback(reply->readAll());

        m->deleteLater();
    });
}

void QtPushBullet::postApi(const QUrl &url, QJsonDocument json,
                           const std::function<void(const QByteArray &data)> &callback)
{
    QNetworkAccessManager *m = new QNetworkAccessManager;
    QNetworkRequest request(url);
    request.setRawHeader(QByteArray("Access-Token"), m_authToken.toLatin1());
    request.setHeader(QNetworkRequest::ContentTypeHeader, QByteArray("application/json"));

    m->post(request, json.toJson());

    connect(m, &QNetworkAccessManager::finished, [=](QNetworkReply *reply)
    {
        if(callback)
            callback(reply->readAll());

        m->deleteLater();
    });
}

void QtPushBullet::uploadFile(const QString &uploadUrl,
                              const QString &fileName,
                              const std::function<void(const QByteArray &)> &callback)
{
    if(!QFile::exists(fileName))
        return;

    QNetworkAccessManager *m = new QNetworkAccessManager;
    QNetworkRequest request(QUrl::fromUserInput(uploadUrl));

    QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

    QHttpPart filePart;
    QFile file(fileName);
    if(file.open(QIODevice::ReadOnly))
    {
        QByteArray fileData = file.readAll();
        file.close();

        filePart.setBody(fileData);

        filePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                           QString("form-data; name=\"file\"; filename=\"%0\"")
                           .arg(QFileInfo(fileName).fileName()));

        multiPart->append(filePart);
    }
    else
    {
        delete multiPart;
        delete m;
        return;
    }

    QNetworkReply *reply = m->post(request, multiPart);
    multiPart->setParent(reply);

    QObject::connect(m, &QNetworkAccessManager::finished, [=]
    {
        if(callback)
            callback(reply->readAll());

        if(reply)
            reply->deleteLater();

        if(m)
            m->deleteLater();
    });
}
