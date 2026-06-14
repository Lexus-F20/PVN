#include "connectionUiController.h"

#if defined(Q_OS_ANDROID) || defined(Q_OS_IOS) || defined(MACOS_NE)
    #include <QGuiApplication>
#else
    #include <QApplication>
#endif

#include <QRegularExpression>

#include "amneziaApplication.h"
#include "core/controllers/serversController.h"
#include "core/models/containerConfig.h"
#include "core/utils/containerEnum.h"
#include "vpnConnection.h"

ConnectionUiController::ConnectionUiController(ConnectionController* connectionController,
                                                ServersController* serversController,
                                                QObject *parent)
    : QObject(parent),
      m_connectionController(connectionController),
      m_serversController(serversController)
{
    connect(m_connectionController, &ConnectionController::connectionStateChanged, this, &ConnectionUiController::onConnectionStateChanged);
    connect(m_connectionController, &ConnectionController::bytesChanged, this, &ConnectionUiController::onBytesChanged);

    connect(this, &ConnectionUiController::connectButtonClicked, this, &ConnectionUiController::toggleConnection, Qt::QueuedConnection);

    m_pingTimer.setInterval(3000);
    connect(&m_pingTimer, &QTimer::timeout, this, &ConnectionUiController::measurePing);

    m_state = Vpn::ConnectionState::Disconnected;
}

void ConnectionUiController::openConnection()
{
    const QString serverId = m_serversController->getDefaultServerId();
    if (serverId.isEmpty()) {
        return;
    }

    ErrorCode errorCode = m_connectionController->openConnection(serverId);

    if (errorCode != ErrorCode::NoError) {
        notifyConnectionBlocked(errorCode);
        return;
    }
}

void ConnectionUiController::closeConnection()
{
    m_connectionController->closeConnection();
}

ErrorCode ConnectionUiController::getLastConnectionError()
{
    return m_connectionController->lastConnectionError();
}

void ConnectionUiController::onConnectionStateChanged(Vpn::ConnectionState state)
{
    m_state = state;

    m_isConnected = false;
    m_connectionStateText = tr("Connecting...");
    switch (state) {
    case Vpn::ConnectionState::Connected: {
        amnApp->networkManager()->clearConnectionCache();

        m_isConnectionInProgress = false;
        m_isConnected = true;
        m_connectionStateText = tr("Connected");
        m_pingTimer.start();
        QTimer::singleShot(500, this, &ConnectionUiController::measurePing);
        break;
    }
    case Vpn::ConnectionState::Connecting: {
        m_isConnectionInProgress = true;
        break;
    }
    case Vpn::ConnectionState::Reconnecting: {
        m_isConnectionInProgress = true;
        m_connectionStateText = tr("Reconnecting...");
        break;
    }
    case Vpn::ConnectionState::Disconnected: {
        m_isConnectionInProgress = false;
        m_connectionStateText = tr("Connect");
        m_pingTimer.stop();
        m_downloadSpeed = QStringLiteral("0.00 Mbps");
        m_uploadSpeed   = QStringLiteral("0.00 Mbps");
        m_pingMs = -1;
        emit statsChanged();
        break;
    }
    case Vpn::ConnectionState::Disconnecting: {
        m_isConnectionInProgress = true;
        m_connectionStateText = tr("Disconnecting...");
        break;
    }
    case Vpn::ConnectionState::Preparing: {
        m_isConnectionInProgress = true;
        m_connectionStateText = tr("Preparing...");
        break;
    }
    case Vpn::ConnectionState::Error: {
        m_isConnectionInProgress = false;
        m_connectionStateText = tr("Connect");
        emit connectionErrorOccurred(getLastConnectionError());
        break;
    }
    case Vpn::ConnectionState::Unknown: {
        m_isConnectionInProgress = false;
        m_connectionStateText = tr("Connect");
        emit connectionErrorOccurred(getLastConnectionError());
        break;
    }
    }
    emit connectionStateChanged();
}

void ConnectionUiController::onTranslationsUpdated()
{
    onConnectionStateChanged(getCurrentConnectionState());
}

Vpn::ConnectionState ConnectionUiController::getCurrentConnectionState()
{
    return m_state;
}

QString ConnectionUiController::connectionStateText() const
{
    return m_connectionStateText;
}

void ConnectionUiController::toggleConnection()
{
    if (m_state == Vpn::ConnectionState::Preparing) {
        emit preparingConfig();
        return;
    }

    if (isConnectionInProgress()) {
        closeConnection();
    } else if (isConnected()) {
        closeConnection();
    } else {
        const QString serverId = m_serversController->getDefaultServerId();
        if (serverId.isEmpty()) {
            return;
        }

        const ErrorCode errorCode = m_connectionController->isConnectionSupported(serverId);
        if (errorCode != ErrorCode::NoError) {
            notifyConnectionBlocked(errorCode);
            return;
        }

        emit prepareConfig();
    }
}

void ConnectionUiController::notifyConnectionBlocked(ErrorCode errorCode)
{
    if (errorCode == ErrorCode::LegacyApiV1NotSupportedError) {
        emit unsupportedConnectDrawerRequested();
        return;
    }

    if (errorCode == ErrorCode::NoInstalledContainersError) {
        emit noInstalledContainers();
        return;
    }

    emit connectionErrorOccurred(errorCode);
}

bool ConnectionUiController::isConnectionInProgress() const
{
    return m_isConnectionInProgress;
}

bool ConnectionUiController::isConnected() const
{
    return m_isConnected;
}

QString ConnectionUiController::downloadSpeed() const
{
    return m_downloadSpeed;
}

QString ConnectionUiController::uploadSpeed() const
{
    return m_uploadSpeed;
}

int ConnectionUiController::pingMs() const
{
    return m_pingMs;
}

void ConnectionUiController::onBytesChanged(quint64 receivedBytes, quint64 sentBytes)
{
    m_downloadSpeed = VpnConnection::bytesPerSecToText(receivedBytes);
    m_uploadSpeed   = VpnConnection::bytesPerSecToText(sentBytes);
    emit statsChanged();
}

void ConnectionUiController::measurePing()
{
    if (!m_isConnected) return;

    auto *proc = new QProcess(this);
    QStringList args;
#ifdef Q_OS_WIN
    args << "-n" << "1" << "-w" << "1500" << m_gatewayIp;
#else
    args << "-c" << "1" << "-W" << "2"    << m_gatewayIp;
#endif

    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int /*code*/, QProcess::ExitStatus /*status*/) {
        const QString out = QString::fromLocal8Bit(proc->readAllStandardOutput());
        // Match localized ping output: English "time=12ms", "time<1ms",
        // Russian "время=12мс", etc. We accept "time" / "время" / "tiempo" / etc.
        // and any single-byte separator before the digits.
        static const QRegularExpression rx(R"((?:time|время|tiempo|tempo|temps|tempo|szeit|tempo|时间)[=<:]\s*([\d.]+))",
                                           QRegularExpression::CaseInsensitiveOption);
        int newPing = -1;
        const auto m = rx.match(out);
        if (m.hasMatch()) {
            newPing = qRound(m.captured(1).toDouble());
        } else {
            // Last-ditch heuristic: any "= 12 ms" / "=12мс" / "<1 ms" pattern
            static const QRegularExpression rxFallback(R"([=<]\s*([\d.]+)\s*(?:ms|мс))",
                                                       QRegularExpression::CaseInsensitiveOption);
            const auto mf = rxFallback.match(out);
            if (mf.hasMatch()) newPing = qRound(mf.captured(1).toDouble());
        }
        if (newPing != m_pingMs) {
            m_pingMs = newPing;
            emit statsChanged();
        }
        proc->deleteLater();
    });

#ifdef Q_OS_WIN
    proc->start(QStringLiteral("ping.exe"), args);
#else
    proc->start(QStringLiteral("ping"), args);
#endif
}

bool ConnectionUiController::isRevokeBlockedDuringActiveConnection(const QString &serverId, int containerIndex,
                                                                   const QString &clientId) const
{
    if (clientId.isEmpty() || (!isConnected() && !isConnectionInProgress())) {
        return false;
    }

    if (m_serversController->getDefaultServerId() != serverId) {
        return false;
    }

    if (static_cast<int>(m_serversController->getDefaultContainer(serverId)) != containerIndex) {
        return false;
    }

    const auto adminConfig = m_serversController->selfHostedAdminConfig(serverId);
    if (!adminConfig.has_value()) {
        return false;
    }

    const QString connectionClientId =
            adminConfig->containerConfig(static_cast<DockerContainer>(containerIndex)).protocolConfig.clientId();
    if (connectionClientId.isEmpty()) {
        return false;
    }

    return connectionClientId == clientId || connectionClientId.contains(clientId);
}
