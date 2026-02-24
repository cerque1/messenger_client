#include "callsession.h"

CallSession::CallSession(QObject* parent)
    : QObject(parent)
{
}

void CallSession::setupPeerConnection() {
#ifndef HAVE_LIBDATACHANNEL
    emit error("libdatachannel недоступна в этой сборке");
#else
    rtc::Configuration config;
    peer_connection_ = std::make_shared<rtc::PeerConnection>(config);

    peer_connection_->onStateChange([this](rtc::PeerConnection::State state) {
        if (state == rtc::PeerConnection::State::Connected) {
            emit connected();
        } else if (state == rtc::PeerConnection::State::Disconnected ||
                   state == rtc::PeerConnection::State::Failed ||
                   state == rtc::PeerConnection::State::Closed) {
            emit disconnected();
        }
    });

    peer_connection_->onLocalDescription([this](rtc::Description description) {
        const QString sdp = QString::fromStdString(static_cast<std::string>(description));
        if (description.type() == rtc::Description::Type::Offer) {
            emit localOfferCreated(sdp);
        } else if (description.type() == rtc::Description::Type::Answer) {
            emit localAnswerCreated(sdp);
        }
    });

    peer_connection_->onLocalCandidate([this](rtc::Candidate candidate) {
        emit localCandidateCreated(
            QString::fromStdString(candidate.candidate()),
            QString::fromStdString(candidate.mid()),
            0);
    });

    heartbeat_channel_ = peer_connection_->createDataChannel("call-heartbeat");
#endif
}

bool CallSession::startOutgoing() {
    setupPeerConnection();
#ifdef HAVE_LIBDATACHANNEL
    peer_connection_->setLocalDescription();
    return true;
#else
    return false;
#endif
}

bool CallSession::startIncoming(const QString& remoteOffer) {
    setupPeerConnection();
#ifdef HAVE_LIBDATACHANNEL
    peer_connection_->setRemoteDescription(rtc::Description(remoteOffer.toStdString(), "offer"));
    peer_connection_->setLocalDescription();
    return true;
#else
    Q_UNUSED(remoteOffer);
    return false;
#endif
}

bool CallSession::applyRemoteAnswer(const QString& remoteAnswer) {
#ifdef HAVE_LIBDATACHANNEL
    if (!peer_connection_) {
        emit error("PeerConnection не инициализирован");
        return false;
    }
    peer_connection_->setRemoteDescription(rtc::Description(remoteAnswer.toStdString(), "answer"));
    return true;
#else
    Q_UNUSED(remoteAnswer);
    return false;
#endif
}

bool CallSession::addRemoteCandidate(const QString& candidate, const QString& mid, int mLineIndex) {
#ifdef HAVE_LIBDATACHANNEL
    Q_UNUSED(mLineIndex);
    if (!peer_connection_) {
        emit error("PeerConnection не инициализирован");
        return false;
    }
    peer_connection_->addRemoteCandidate(
        rtc::Candidate(candidate.toStdString(), mid.toStdString()));
    return true;
#else
    Q_UNUSED(candidate);
    Q_UNUSED(mid);
    Q_UNUSED(mLineIndex);
    return false;
#endif
}

void CallSession::close() {
#ifdef HAVE_LIBDATACHANNEL
    heartbeat_channel_.reset();
    peer_connection_.reset();
#endif
}
