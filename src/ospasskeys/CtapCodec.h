/*
 *  Copyright (C) 2026 KeePassXC Team <team@keepassxc.org>
 *
 *  Decode/encode a practical subset of CTAP2 CBOR used by Windows plugin authenticators.
 */

#ifndef KEEPASSXC_CTAPCODEC_H
#define KEEPASSXC_CTAPCODEC_H

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace CtapCodec
{

bool decodeMakeCredential(const QByteArray& ctapCbor, QJsonObject* publicKeyOptions, QString* origin, QString* error);

bool decodeGetAssertion(const QByteArray& ctapCbor, QJsonObject* publicKeyOptions, QString* origin, QString* error);

bool getAssertionHasAllowList(const QByteArray& ctapCbor);

int getAssertionAllowListSize(const QByteArray& ctapCbor);

bool getAssertionAllowListUsesStringKeys(const QByteArray& ctapCbor);

QString getAssertionRpId(const QByteArray& ctapCbor);

QByteArray encodeMakeCredentialResponse(const QJsonObject& webauthnResponse);
QByteArray encodeGetAssertionResponse(const QJsonObject& webauthnResponse,
                                      int allowListSize = 0,
                                      bool stringCredKeys = true);

QByteArray authenticatorGetInfoCbor();
}

#endif
