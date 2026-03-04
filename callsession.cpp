#include "callsession.h"

#include <QMetaObject>
#include <cstring>
#include <exception>

CallSession::CallSession(QObject* parent)
    : QObject(parent)
{
}

void CallSession::configureMediaChannel(const std::shared_ptr<rtc::DataChannel>& channel, bool receiveMessages) {
#ifdef HAVE_LIBDATACHANNEL
    if (!channel) {
        return;
    }

    media_channel_open_.store(channel->isOpen());

    channel->onOpen([this]() {
        media_channel_open_.store(true);
        flushPendingMediaPackets();
    });

    channel->onClosed([this]() {
        const bool anyChannelOpen =
            (outgoing_media_channel_ && outgoing_media_channel_->isOpen()) ||
            (incoming_media_channel_ && incoming_media_channel_->isOpen());
        media_channel_open_.store(anyChannelOpen);
    });

    if (receiveMessages) {
        channel->onMessage([this](rtc::message_variant message) {
            if (const rtc::binary* payload = std::get_if<rtc::binary>(&message)) {
                QByteArray packet(reinterpret_cast<const char*>(payload->data()), static_cast<int>(payload->size()));
                QMetaObject::invokeMethod(this, [this, packet]() {
                    emit mediaPacketReceived(packet);
                }, Qt::QueuedConnection);
            }
        });
    }
#endif
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
            flushPendingMediaPackets();
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

    peer_connection_->onDataChannel([this](std::shared_ptr<rtc::DataChannel> channel) {
        if (!channel || channel->label() != "call-media") {
            return;
        }

        incoming_media_channel_ = channel;
        configureMediaChannel(incoming_media_channel_, true);
        flushPendingMediaPackets();
    });

    media_channel_open_.store(false);
    heartbeat_channel_.reset();
    outgoing_media_channel_.reset();
    incoming_media_channel_.reset();

    heartbeat_channel_ = peer_connection_->createDataChannel("call-heartbeat");
    outgoing_media_channel_ = peer_connection_->createDataChannel("call-media");
    configureMediaChannel(outgoing_media_channel_, false);
    flushPendingMediaPackets();
#endif
}

void CallSession::flushPendingRemoteCandidates() {
#ifdef HAVE_LIBDATACHANNEL
    if (!peer_connection_ || !remote_description_set_) {
        return;
    }

    for (const PendingCandidate& pending : pending_remote_candidates_) {
        try {
            peer_connection_->addRemoteCandidate(
                rtc::Candidate(pending.candidate.toStdString(), pending.mid.toStdString()));
        } catch (const std::exception& ex) {
            emit error(QString("Не удалось добавить отложенный ICE candidate: %1").arg(ex.what()));
        }
    }
    pending_remote_candidates_.clear();
#endif
}

std::shared_ptr<rtc::DataChannel> CallSession::getWritableMediaChannel() const {
#ifdef HAVE_LIBDATACHANNEL
    if (outgoing_media_channel_ && outgoing_media_channel_->isOpen()) {
        return outgoing_media_channel_;
    }
    if (incoming_media_channel_ && incoming_media_channel_->isOpen()) {
        return incoming_media_channel_;
    }
    if (outgoing_media_channel_) {
        return outgoing_media_channel_;
    }
    return incoming_media_channel_;
#else
    return nullptr;
#endif
}

void CallSession::flushPendingMediaPackets() {
#ifdef HAVE_LIBDATACHANNEL
    std::shared_ptr<rtc::DataChannel> channel = getWritableMediaChannel();
    if (!channel) {
        return;
    }

    while (!pending_media_packets_.empty()) {
        const QByteArray& packet = pending_media_packets_.front();
        if (packet.isEmpty()) {
            pending_media_packets_.pop_front();
            continue;
        }

        rtc::binary payload;
        payload.resize(static_cast<size_t>(packet.size()));
        std::memcpy(payload.data(), packet.constData(), static_cast<size_t>(packet.size()));

        try {
            channel->send(std::move(payload));
            media_channel_open_.store(true);
            pending_media_packets_.pop_front();
        } catch (const std::exception&) {
            media_channel_open_.store(channel->isOpen());
            break;
        }

        channel = getWritableMediaChannel();
        if (!channel) {
            break;
        }
    }
#endif
}

bool CallSession::startOutgoing() {
    setupPeerConnection();
#ifdef HAVE_LIBDATACHANNEL
    remote_description_set_ = false;
    pending_remote_candidates_.clear();
    peer_connection_->setLocalDescription();
    return true;
#else
    return false;
#endif
}

bool CallSession::startIncoming(const QString& remoteOffer) {
    setupPeerConnection();
#ifdef HAVE_LIBDATACHANNEL
    remote_description_set_ = false;
    pending_remote_candidates_.clear();

    try {
        peer_connection_->setRemoteDescription(rtc::Description(remoteOffer.toStdString(), "offer"));
        remote_description_set_ = true;
        flushPendingRemoteCandidates();
        peer_connection_->setLocalDescription();
    } catch (const std::exception& ex) {
        emit error(QString("Не удалось применить удалённый offer: %1").arg(ex.what()));
        return false;
    }

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

    try {
        peer_connection_->setRemoteDescription(rtc::Description(remoteAnswer.toStdString(), "answer"));
        remote_description_set_ = true;
        flushPendingRemoteCandidates();
        flushPendingMediaPackets();
    } catch (const std::exception& ex) {
        emit error(QString("Не удалось применить удалённый answer: %1").arg(ex.what()));
        return false;
    }

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

    if (!remote_description_set_) {
        pending_remote_candidates_.push_back(PendingCandidate{candidate, mid, mLineIndex});
        return true;
    }

    try {
        peer_connection_->addRemoteCandidate(
            rtc::Candidate(candidate.toStdString(), mid.toStdString()));
    } catch (const std::exception& ex) {
        emit error(QString("Не удалось добавить ICE candidate: %1").arg(ex.what()));
        return false;
    }

    return true;
#else
    Q_UNUSED(candidate);
    Q_UNUSED(mid);
    Q_UNUSED(mLineIndex);
    return false;
#endif
}

bool CallSession::sendMediaPacket(const QByteArray& packet) {
#ifdef HAVE_LIBDATACHANNEL
    if (packet.isEmpty()) {
        return false;
    }

    auto enqueuePending = [this, &packet]() {
        pending_media_packets_.push_back(packet);
        constexpr size_t kMaxPendingMediaPackets = 200;
        if (pending_media_packets_.size() > kMaxPendingMediaPackets) {
            pending_media_packets_.pop_front();
        }
    };

    std::shared_ptr<rtc::DataChannel> channel = getWritableMediaChannel();
    if (!channel) {
        enqueuePending();
        return true;
    }

    rtc::binary payload;
    payload.resize(static_cast<size_t>(packet.size()));
    std::memcpy(payload.data(), packet.constData(), static_cast<size_t>(packet.size()));

    try {
        channel->send(std::move(payload));
        media_channel_open_.store(true);
        flushPendingMediaPackets();
    } catch (const std::exception&) {
        media_channel_open_.store(channel->isOpen());
        enqueuePending();
    }

    return true;
#else
    Q_UNUSED(packet);
    return false;
#endif
}

void CallSession::close() {
#ifdef HAVE_LIBDATACHANNEL
    pending_remote_candidates_.clear();
    pending_media_packets_.clear();
    remote_description_set_ = false;
    media_channel_open_.store(false);
    incoming_media_channel_.reset();
    outgoing_media_channel_.reset();
    heartbeat_channel_.reset();
    peer_connection_.reset();
#endif
}
