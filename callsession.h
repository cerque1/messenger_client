#ifndef CALLSESSION_H
#define CALLSESSION_H

#include <QObject>
#include <QString>
#include <memory>

#ifdef HAVE_LIBDATACHANNEL
#include <rtc/rtc.hpp>
#endif

class CallSession : public QObject
{
    Q_OBJECT
public:
    explicit CallSession(QObject* parent = nullptr);

    bool startOutgoing();
    bool startIncoming(const QString& remoteOffer);
    bool applyRemoteAnswer(const QString& remoteAnswer);
    bool addRemoteCandidate(const QString& candidate, const QString& mid, int mLineIndex);
    void close();

signals:
    void localOfferCreated(const QString& sdp);
    void localAnswerCreated(const QString& sdp);
    void localCandidateCreated(const QString& candidate, const QString& mid, int mLineIndex);
    void connected();
    void disconnected();
    void error(const QString& message);

private:
    void setupPeerConnection();

#ifdef HAVE_LIBDATACHANNEL
    std::shared_ptr<rtc::PeerConnection> peer_connection_;
    std::shared_ptr<rtc::DataChannel> heartbeat_channel_;
#endif
};

#endif // CALLSESSION_H
