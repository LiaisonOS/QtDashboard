//
// Author  : Sylvain Deguire (VA2OPS)
// Date    : May 2026
// Purpose : IPC client for et-supervisor Unix socket.
//

#include "SupervisorClient.h"
#include <QJsonDocument>
#include <QProcessEnvironment>
#include <QDir>
#include <unistd.h>
#include <pwd.h>

SupervisorClient::SupervisorClient(QObject *parent)
    : QObject(parent)
    , m_socket(new QLocalSocket(this))
{
    uid_t uid = getuid();

    // If running as root via sudo, use the real user's UID instead
    if (uid == 0) {
        QByteArray sudoUser = qgetenv("SUDO_USER");
        if (!sudoUser.isEmpty()) {
            struct passwd *pw = getpwnam(sudoUser.constData());
            if (pw)
                uid = pw->pw_uid;
        }
    }

    m_socketPath = QString("/run/user/%1/et-supervisor.sock").arg(uid);

    connect(m_socket, &QLocalSocket::connected,    this, &SupervisorClient::onConnected);
    connect(m_socket, &QLocalSocket::disconnected, this, &SupervisorClient::onDisconnected);
    connect(m_socket, &QLocalSocket::readyRead,    this, &SupervisorClient::onReadyRead);
    connect(m_socket, &QLocalSocket::errorOccurred, this, &SupervisorClient::onError);

    connectToSupervisor();
}

void SupervisorClient::connectToSupervisor()
{
    m_socket->connectToServer(m_socketPath);
}

void SupervisorClient::startMode(const QString &modeId, const QJsonObject &params)
{
    QJsonObject cmd;
    cmd["cmd"]  = "start-mode";
    cmd["mode"] = modeId;
    if (!params.isEmpty())
        cmd["params"] = params;
    sendCommand(cmd);
}

void SupervisorClient::stopAll()
{
    QJsonObject cmd;
    cmd["cmd"] = "stop";
    sendCommand(cmd);
}

void SupervisorClient::requestStatus()
{
    QJsonObject cmd;
    cmd["cmd"] = "status";
    sendCommand(cmd);
}

void SupervisorClient::sendCommand(const QJsonObject &cmd)
{
    if (m_socket->state() != QLocalSocket::ConnectedState) {
        m_pending.enqueue(cmd);
        if (m_socket->state() == QLocalSocket::UnconnectedState)
            connectToSupervisor();
        return;
    }
    QByteArray data = QJsonDocument(cmd).toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
    m_socket->flush();
}

void SupervisorClient::onConnected()
{
    emit connected();
    // Flush any commands queued while we were reconnecting
    while (!m_pending.isEmpty())
        sendCommand(m_pending.dequeue());
    requestStatus();
}

void SupervisorClient::onDisconnected()
{
    emit disconnected();
}

void SupervisorClient::onReadyRead()
{
    m_buffer += m_socket->readAll();

    while (m_buffer.contains('\n')) {
        int idx = m_buffer.indexOf('\n');
        QByteArray line = m_buffer.left(idx).trimmed();
        m_buffer = m_buffer.mid(idx + 1);

        if (line.isEmpty())
            continue;

        QJsonDocument doc = QJsonDocument::fromJson(line);
        if (!doc.isNull() && doc.isObject())
            emit statusReceived(doc.object());
    }
}

void SupervisorClient::onError(QLocalSocket::LocalSocketError err)
{
    Q_UNUSED(err)
    emit error(m_socket->errorString());
}
