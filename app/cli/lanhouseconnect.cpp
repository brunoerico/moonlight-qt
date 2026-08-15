#include "lanhouseconnect.h"

#include "backend/computermanager.h"
#include "backend/computerseeker.h"
#include "streaming/session.h"

#include <QCoreApplication>
#include <QTimer>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>

#define COMPUTER_SEEK_TIMEOUT 30000
#define APP_SEEK_TIMEOUT 10000

// No existing LanHouse-specific config/settings layer in this codebase to
// pull this from (unlike the WebRTC bridge, which has web_server.lanhouse_web_base_url) -
// hardcoded the same way mappingfetcher.cpp hardcodes moonlight-stream.org.
static const QString LANHOUSE_WEB_BASE_URL = "https://lanhousecloudgaming.com.br";

namespace LanhouseConnect
{

// Reports a freshly-generated pairing PIN to lanhouse-web instead of
// displaying it - mirrors host.rs::report_pin_to_lanhouse_web on the WebRTC
// bridge side (Fase 3). Best-effort and fire-and-forget: the caller doesn't
// wait on this, pairing completion is still driven entirely by
// ComputerManager's own pairingCompleted signal, exactly as before.
static void reportPinToLanhouseWeb(const QString &ticket, const QString &lanhouseHostId, const QString &pin)
{
    auto nam = new QNetworkAccessManager();
    nam->setStrictTransportSecurityEnabled(true);

    QUrl url(LANHOUSE_WEB_BASE_URL + "/api/hosts/pairing/request");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject body;
    body["ticket"] = ticket;
    body["hostId"] = lanhouseHostId;
    body["pin"] = pin;
    body["clientLabel"] = "LanHouse Native";

    QNetworkReply *reply = nam->post(request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QObject::connect(reply, &QNetworkReply::finished, [nam, reply]() {
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "Failed to report pairing PIN to lanhouse-web:" << reply->errorString();
        }
        reply->deleteLater();
        nam->deleteLater();
    });
}

enum State {
    StateInit,
    StateSeekComputer,
    StatePairing,
    StateSeekApp,
    StateStartSession,
    StateFailure,
};

class Event
{
public:
    enum Type {
        AppQuitCompleted,
        AppQuitRequested,
        ComputerFound,
        ComputerUpdated,
        Executed,
        PairingCompleted,
        Timedout,
    };

    Event(Type type)
        : type(type), computerManager(nullptr), computer(nullptr) {}

    Type type;
    ComputerManager *computerManager;
    NvComputer *computer;
    QString errorMessage;
};

class LauncherPrivate
{
    Q_DECLARE_PUBLIC(Launcher)

public:
    LauncherPrivate(Launcher *q) : q_ptr(q) {}

    void handleEvent(Event event)
    {
        Q_Q(Launcher);

        switch (event.type) {
        case Event::Executed:
            if (m_State == StateInit) {
                m_State = StateSeekComputer;
                m_ComputerManager = event.computerManager;

                q->connect(m_ComputerManager, &ComputerManager::pairingCompleted,
                           q, &Launcher::onPairingCompleted);
                q->connect(m_ComputerManager, &ComputerManager::computerStateChanged,
                           q, &Launcher::onComputerUpdated);
                q->connect(m_ComputerManager, &ComputerManager::quitAppCompleted,
                           q, &Launcher::onQuitAppCompleted);

                m_ComputerSeeker = new ComputerSeeker(m_ComputerManager, m_ComputerName, q);
                q->connect(m_ComputerSeeker, &ComputerSeeker::computerFound,
                           q, &Launcher::onComputerFound);
                q->connect(m_ComputerSeeker, &ComputerSeeker::errorTimeout,
                           q, &Launcher::onTimeout);
                m_ComputerSeeker->start(COMPUTER_SEEK_TIMEOUT);

                emit q->searchingComputer();
            }
            break;
        case Event::ComputerFound:
            if (m_State == StateSeekComputer) {
                m_Computer = event.computer;
                if (event.computer->pairState == NvComputer::PS_PAIRED) {
                    m_State = StateSeekApp;
                    m_TimeoutTimer->start(APP_SEEK_TIMEOUT);
                    emit q->searchingApp();
                }
                else {
                    QString pin = m_ComputerManager->generatePinString();
                    m_State = StatePairing;
                    m_ComputerManager->pairHost(event.computer, pin);
                    reportPinToLanhouseWeb(m_Ticket, m_LanhouseHostId, pin);
                    emit q->pairing();
                }
            }
            break;
        case Event::PairingCompleted:
            if (m_State == StatePairing) {
                if (event.errorMessage.isEmpty()) {
                    m_State = StateSeekApp;
                    m_TimeoutTimer->start(APP_SEEK_TIMEOUT);
                    emit q->searchingApp();
                    // The computer we already have may be stale (just paired) -
                    // force a refresh so the app list is populated.
                    onComputerUpdatedOrRecheck(event.computer);
                }
                else {
                    m_State = StateFailure;
                    emit q->failed(event.errorMessage);
                }
            }
            break;
        case Event::ComputerUpdated:
            if (m_State == StateSeekApp) {
                onComputerUpdatedOrRecheck(event.computer);
            }
            break;
        case Event::AppQuitRequested:
            if (m_State == StateSeekApp) {
                m_ComputerManager->quitRunningApp(m_Computer);
            }
            break;
        case Event::AppQuitCompleted:
            if (m_State == StateSeekApp && !event.errorMessage.isEmpty()) {
                m_State = StateFailure;
                emit q->failed(QObject::tr("Quitting app failed, reason: %1").arg(event.errorMessage));
            }
            break;
        case Event::Timedout:
            if (m_State == StateSeekComputer) {
                m_State = StateFailure;
                emit q->failed(QObject::tr("Failed to connect to %1").arg(m_ComputerName));
            }
            if (m_State == StateSeekApp) {
                m_State = StateFailure;
                emit q->failed(QObject::tr("Failed to find application %1").arg(m_AppName));
            }
            break;
        }
    }

    // Shared between the ComputerUpdated event and the recheck right after
    // pairing completes (the computer's own state-changed signal may or may
    // not have already fired by the time pairing finishes).
    void onComputerUpdatedOrRecheck(NvComputer *computer)
    {
        Q_Q(Launcher);
        Session* session;
        NvApp app;

        if (computer != nullptr) {
            m_Computer = computer;
        }
        int index = getAppIndex();
        if (-1 != index) {
            app = m_Computer->appList[index];
            m_TimeoutTimer->stop();
            if (isNotStreaming() || isStreamingApp(app)) {
                m_State = StateStartSession;
                session = new Session(m_Computer, app, m_Preferences);
                emit q->sessionCreated(app.name, session);
            } else {
                emit q->appQuitRequired(getCurrentAppName());
            }
        }
    }

    int getAppIndex() const
    {
        for (int i = 0; i < m_Computer->appList.length(); i++) {
            if (m_Computer->appList[i].name.toLower() == m_AppName.toLower()) {
                return i;
            }
        }
        return -1;
    }

    bool isNotStreaming() const
    {
        return m_Computer->currentGameId == 0;
    }

    bool isStreamingApp(NvApp app) const
    {
        return m_Computer->currentGameId == app.id;
    }

    QString getCurrentAppName() const
    {
        for (const NvApp& app : std::as_const(m_Computer->appList)) {
            if (m_Computer->currentGameId == app.id) {
                return app.name;
            }
        }
        return "<UNKNOWN>";
    }

    Launcher *q_ptr;
    QString m_ComputerName;
    QString m_AppName;
    QString m_Ticket;
    QString m_LanhouseHostId;
    StreamingPreferences *m_Preferences;
    ComputerManager *m_ComputerManager;
    ComputerSeeker *m_ComputerSeeker;
    NvComputer *m_Computer;
    State m_State;
    QTimer *m_TimeoutTimer;
};

Launcher::Launcher(QString computer, QString app, QString ticket, QString lanhouseHostId,
                   StreamingPreferences* preferences, QObject *parent)
    : QObject(parent),
      m_DPtr(new LauncherPrivate(this))
{
    Q_D(Launcher);
    d->m_ComputerName = computer;
    d->m_AppName = app;
    d->m_Ticket = ticket;
    d->m_LanhouseHostId = lanhouseHostId;
    d->m_Preferences = preferences;
    d->m_State = StateInit;
    d->m_TimeoutTimer = new QTimer(this);
    d->m_TimeoutTimer->setSingleShot(true);
    connect(d->m_TimeoutTimer, &QTimer::timeout,
            this, &Launcher::onTimeout);
}

Launcher::~Launcher()
{
}

void Launcher::execute(ComputerManager *manager)
{
    Q_D(Launcher);
    Event event(Event::Executed);
    event.computerManager = manager;
    d->handleEvent(event);
}

void Launcher::quitRunningApp()
{
    Q_D(Launcher);
    Event event(Event::AppQuitRequested);
    d->handleEvent(event);
}

bool Launcher::isExecuted() const
{
    Q_D(const Launcher);
    return d->m_State != StateInit;
}

void Launcher::onComputerFound(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerFound);
    event.computer = computer;
    d->handleEvent(event);
}

void Launcher::onComputerUpdated(NvComputer *computer)
{
    Q_D(Launcher);
    Event event(Event::ComputerUpdated);
    event.computer = computer;
    d->handleEvent(event);
}

void Launcher::onPairingCompleted(NvComputer *computer, QString error)
{
    Q_D(Launcher);
    Event event(Event::PairingCompleted);
    event.computer = computer;
    event.errorMessage = error;
    d->handleEvent(event);
}

void Launcher::onTimeout()
{
    Q_D(Launcher);
    Event event(Event::Timedout);
    d->handleEvent(event);
}

void Launcher::onQuitAppCompleted(QVariant error)
{
    Q_D(Launcher);
    Event event(Event::AppQuitCompleted);
    event.errorMessage = error.toString();
    d->handleEvent(event);
}

}
