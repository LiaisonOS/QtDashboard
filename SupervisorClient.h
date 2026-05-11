//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : IPC client for et-supervisor Unix socket.
//           Mirrors the Python SupervisorClient — sends JSON commands,
//           receives JSON responses. Uses QLocalSocket (non-blocking).
//

#pragma once

#include <QObject>
#include <QLocalSocket>
#include <QJsonObject>
#include <QString>
#include <QQueue>

class SupervisorClient : public QObject
{
    Q_OBJECT

public:
    explicit SupervisorClient(QObject *parent = nullptr);

    void startMode(const QString &modeId, const QJsonObject &params = QJsonObject());
    void stopAll();
    void requestStatus();

signals:
    void statusReceived(const QJsonObject &status);
    void connected();
    void disconnected();
    void error(const QString &msg);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyRead();
    void onError(QLocalSocket::LocalSocketError err);

private:
    void sendCommand(const QJsonObject &cmd);
    void connectToSupervisor();

    QLocalSocket        *m_socket;
    QString              m_socketPath;
    QByteArray           m_buffer;
    QQueue<QJsonObject>  m_pending;   // commands queued while reconnecting
};
