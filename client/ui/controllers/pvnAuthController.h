#ifndef PVNAUTHCONTROLLER_H
#define PVNAUTHCONTROLLER_H

#include <QObject>
#include <QTcpServer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QString>
#include <QJsonObject>

#include "core/controllers/selfhosted/importController.h"
#include "core/controllers/connectionController.h"

class PvnAuthController : public QObject
{
    Q_OBJECT
public:
    enum class State {
        Idle,
        Opening,        // Browser opening / waiting for user
        ExchangingCode, // Got code, exchanging for id_token
        CallingBackend, // Got id_token, calling PVN worker
        Importing,      // Importing returned wg config
        Authenticated,  // Done, signed in
        Failed,
    };
    Q_ENUM(State)

    Q_PROPERTY(int state READ stateInt NOTIFY stateChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY stateChanged)
    Q_PROPERTY(QString userEmail READ userEmail NOTIFY userChanged)
    Q_PROPERTY(QString userName READ userName NOTIFY userChanged)
    Q_PROPERTY(bool isSignedIn READ isSignedIn NOTIFY userChanged)

    explicit PvnAuthController(ImportController *importController,
                               ConnectionController *connectionController,
                               QObject *parent = nullptr);

    int stateInt() const { return static_cast<int>(m_state); }
    QString errorMessage() const { return m_errorMessage; }
    QString userEmail() const { return m_userEmail; }
    QString userName() const { return m_userName; }
    bool isSignedIn() const { return !m_jwt.isEmpty(); }

public slots:
    void signInWithGoogle();
    void signInWithEmail(const QString &email, const QString &password);
    void registerWithEmail(const QString &email, const QString &password, const QString &name);
    void signOut();

signals:
    void stateChanged();
    void userChanged();
    void signInCompleted();
    void signInFailed(const QString &message);

private:
    void setState(State s);
    void fail(const QString &msg);
    bool startLocalCallbackServer();
    void onCallbackConnection();
    void handleCallback(const QString &code, const QString &returnedState);
    void exchangeCodeForIdToken(const QString &code);
    void callBackendAuth(const QString &idToken);
    void postBackendAuth(const QString &endpoint, const QJsonObject &body);
    void handleAuthReply(QNetworkReply *reply);
    void importReturnedConfig(const QString &configText);
    void loadPersistedSession();
    void persistSession();
    void clearPersistedSession();
    void maybeRefreshSession();      // called on startup if JWT near expiry
    void postBackendLogout();        // fire-and-forget /me/logout with current JWT
    static QString randomBase64Url(int bytes);
    static QString sha256Base64Url(const QString &input);
    static qint64 jwtExpiryFromToken(const QString &jwt);  // Unix seconds, 0 if unknown
    QString deviceId() const;

    State m_state = State::Idle;
    QString m_errorMessage;
    QString m_userEmail;
    QString m_userName;
    QString m_jwt;

    QTcpServer m_callbackServer;
    quint16 m_callbackPort = 0;
    QString m_pkceVerifier;
    QString m_oauthState;
    QString m_redirectUri;

    QNetworkAccessManager m_net;

    ImportController *m_importController;
    ConnectionController *m_connectionController;
};

#endif // PVNAUTHCONTROLLER_H
