/*
 *  Copyright (c) 2016-2017, The OpenThread Authors.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements MechCop TLV helper functions.
 */

#include "meshcop_tlvs.hpp"

#include "common/const_cast.hpp"
#include "common/debug.hpp"
#include "common/num_utils.hpp"
#include "common/numeric_limits.hpp"
#include "common/string.hpp"
#include "meshcop/meshcop.hpp"

namespace ot {
namespace MeshCoP {

bool Tlv::IsValid(const Tlv &aTlv)
{
    bool    isValid   = true;
    uint8_t minLength = 0;

    switch (aTlv.GetType())
    {
    case Tlv::kPanId:
        minLength = sizeof(PanIdTlv::UintValueType);
        break;
    case Tlv::kExtendedPanId:
        minLength = sizeof(ExtendedPanIdTlv::ValueType);
        break;
    case Tlv::kPskc:
        minLength = sizeof(PskcTlv::ValueType);
        break;
    case Tlv::kNetworkKey:
        minLength = sizeof(NetworkKeyTlv::ValueType);
        break;
    case Tlv::kMeshLocalPrefix:
        minLength = sizeof(MeshLocalPrefixTlv::ValueType);
        break;
    case Tlv::kChannel:
        VerifyOrExit(aTlv.GetLength() >= sizeof(ChannelTlvValue), isValid = false);
        isValid = aTlv.ReadValueAs<ChannelTlv>().IsValid();
        break;
    case Tlv::kNetworkName:
        isValid = As<NetworkNameTlv>(aTlv).IsValid();
        break;

    case Tlv::kSecurityPolicy:
        isValid = As<SecurityPolicyTlv>(aTlv).IsValid();
        break;

    case Tlv::kChannelMask:
        isValid = As<ChannelMaskTlv>(aTlv).IsValid();
        break;

    default:
        break;
    }

    if (minLength > 0)
    {
        isValid = (aTlv.GetLength() >= minLength);
    }

exit:
    return isValid;
}

NameData NetworkNameTlv::GetNetworkName(void) const
{
    uint8_t len = GetLength();

    if (len > sizeof(mNetworkName))
    {
        len = sizeof(mNetworkName);
    }

    return NameData(mNetworkName, len);
}

void NetworkNameTlv::SetNetworkName(const NameData &aNameData)
{
    uint8_t len;

    len = aNameData.CopyTo(mNetworkName, sizeof(mNetworkName));
    SetLength(len);
}

bool NetworkNameTlv::IsValid(void) const { return IsValidUtf8String(mNetworkName, GetLength()); }

void SteeringDataTlv::CopyTo(SteeringData &aSteeringData) const
{
    aSteeringData.Init(GetSteeringDataLength());
    memcpy(aSteeringData.GetData(), mSteeringData, GetSteeringDataLength());
}

bool SecurityPolicyTlv::IsValid(void) const
{
    return GetLength() >= sizeof(mRotationTime) && GetFlagsLength() >= kThread11FlagsLength;
}

SecurityPolicy SecurityPolicyTlv::GetSecurityPolicy(void) const
{
    SecurityPolicy securityPolicy;
    uint8_t        length = Min(static_cast<uint8_t>(sizeof(mFlags)), GetFlagsLength());

    securityPolicy.mRotationTime = GetRotationTime();
    securityPolicy.SetFlags(mFlags, length);

    return securityPolicy;
}

void SecurityPolicyTlv::SetSecurityPolicy(const SecurityPolicy &aSecurityPolicy)
{
    SetRotationTime(aSecurityPolicy.mRotationTime);
    aSecurityPolicy.GetFlags(mFlags, sizeof(mFlags));
}

const char *StateTlv::StateToString(State aState)
{
    static const char *const kStateStrings[] = {
        "Pending", // (0) kPending,
        "Accept",  // (1) kAccept
        "Reject",  // (2) kReject,
    };

    static_assert(0 == kPending, "kPending value is incorrect");
    static_assert(1 == kAccept, "kAccept value is incorrect");

    return aState == kReject ? kStateStrings[2] : kStateStrings[aState];
}

bool ChannelMaskBaseTlv::IsValid(void) const
{
    const ChannelMaskEntryBase *cur = GetFirstEntry();
    const ChannelMaskEntryBase *end = reinterpret_cast<const ChannelMaskEntryBase *>(GetNext());
    bool                        ret = false;

    VerifyOrExit(cur != nullptr);

    while (cur < end)
    {
        uint8_t channelPage;

        VerifyOrExit((cur + 1) <= end && cur->GetNext() <= end);

        channelPage = cur->GetChannelPage();

#if OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_SUPPORT
        if (channelPage == OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_CHANNEL_PAGE)
#else
        if ((channelPage == OT_RADIO_CHANNEL_PAGE_0) || (channelPage == OT_RADIO_CHANNEL_PAGE_2))
#endif
        {
            VerifyOrExit(static_cast<const ChannelMaskEntry *>(cur)->IsValid());
        }

        cur = cur->GetNext();
    }

    ret = true;

exit:
    return ret;
}

const ChannelMaskEntryBase *ChannelMaskBaseTlv::GetFirstEntry(void) const
{
    const ChannelMaskEntryBase *entry = nullptr;

    VerifyOrExit(GetLength() >= sizeof(ChannelMaskEntryBase));

    entry = reinterpret_cast<const ChannelMaskEntryBase *>(GetValue());
    VerifyOrExit(GetLength() >= entry->GetEntrySize(), entry = nullptr);

exit:
    return entry;
}

ChannelMaskEntryBase *ChannelMaskBaseTlv::GetFirstEntry(void) { return AsNonConst(AsConst(this)->GetFirstEntry()); }

void ChannelMaskTlv::SetChannelMask(uint32_t aChannelMask)
{
    uint8_t           length = 0;
    ChannelMaskEntry *entry;

    entry = static_cast<ChannelMaskEntry *>(GetFirstEntry());

#if OPENTHREAD_CONFIG_RADIO_915MHZ_OQPSK_SUPPORT
    if (aChannelMask & OT_RADIO_915MHZ_OQPSK_CHANNEL_MASK)
    {
        OT_ASSERT(entry != nullptr);
        entry->Init();
        entry->SetChannelPage(OT_RADIO_CHANNEL_PAGE_2);
        entry->SetMask(aChannelMask & OT_RADIO_915MHZ_OQPSK_CHANNEL_MASK);

        length += sizeof(ChannelMaskEntry);

        entry = static_cast<ChannelMaskEntry *>(entry->GetNext());
    }
#endif

#if OPENTHREAD_CONFIG_RADIO_2P4GHZ_OQPSK_SUPPORT
    if (aChannelMask & OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MASK)
    {
        OT_ASSERT(entry != nullptr);
        entry->Init();
        entry->SetChannelPage(OT_RADIO_CHANNEL_PAGE_0);
        entry->SetMask(aChannelMask & OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MASK);

        length += sizeof(ChannelMaskEntry);
    }
#endif

#if OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_SUPPORT
    if (aChannelMask & OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_CHANNEL_MASK)
    {
        OT_ASSERT(entry != nullptr);
        entry->Init();
        entry->SetChannelPage(OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_CHANNEL_PAGE);
        entry->SetMask(aChannelMask & OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_CHANNEL_MASK);

        length += sizeof(ChannelMaskEntry);
    }
#endif

    SetLength(length);
}

uint32_t ChannelMaskTlv::GetChannelMask(void) const
{
    const ChannelMaskEntryBase *cur  = GetFirstEntry();
    const ChannelMaskEntryBase *end  = reinterpret_cast<const ChannelMaskEntryBase *>(GetNext());
    uint32_t                    mask = 0;

    VerifyOrExit(cur != nullptr);

    while (cur < end)
    {
        uint8_t channelPage;

        VerifyOrExit((cur + 1) <= end && cur->GetNext() <= end);

        channelPage = cur->GetChannelPage();

#if OPENTHREAD_CONFIG_RADIO_915MHZ_OQPSK_SUPPORT
        if (channelPage == OT_RADIO_CHANNEL_PAGE_2)
        {
            mask |= static_cast<const ChannelMaskEntry *>(cur)->GetMask() & OT_RADIO_915MHZ_OQPSK_CHANNEL_MASK;
        }
#endif

#if OPENTHREAD_CONFIG_RADIO_2P4GHZ_OQPSK_SUPPORT
        if (channelPage == OT_RADIO_CHANNEL_PAGE_0)
        {
            mask |= static_cast<const ChannelMaskEntry *>(cur)->GetMask() & OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MASK;
        }
#endif

#if OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_SUPPORT
        if (channelPage == OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_CHANNEL_PAGE)
        {
            mask |= static_cast<const ChannelMaskEntry *>(cur)->GetMask() &
                    OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_CHANNEL_MASK;
        }
#endif

        cur = cur->GetNext();
    }

exit:
    return mask;
}

uint32_t ChannelMaskTlv::GetChannelMask(const Message &aMessage)
{
    uint32_t mask = 0;
    uint16_t offset;
    uint16_t end;

    SuccessOrExit(FindTlvValueStartEndOffsets(aMessage, kChannelMask, offset, end));

    while (offset + sizeof(ChannelMaskEntryBase) <= end)
    {
        ChannelMaskEntry entry;

        IgnoreError(aMessage.Read(offset, entry));
        VerifyOrExit(offset + entry.GetEntrySize() <= end);

        switch (entry.GetChannelPage())
        {
#if OPENTHREAD_CONFIG_RADIO_2P4GHZ_OQPSK_SUPPORT
        case OT_RADIO_CHANNEL_PAGE_0:
            IgnoreError(aMessage.Read(offset, entry));
            mask |= entry.GetMask() & OT_RADIO_2P4GHZ_OQPSK_CHANNEL_MASK;
            break;
#endif

#if OPENTHREAD_CONFIG_RADIO_915MHZ_OQPSK_SUPPORT
        case OT_RADIO_CHANNEL_PAGE_2:
            IgnoreError(aMessage.Read(offset, entry));
            mask |= entry.GetMask() & OT_RADIO_915MHZ_OQPSK_CHANNEL_MASK;
            break;
#endif

#if OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_SUPPORT
        case OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_CHANNEL_PAGE:
            IgnoreError(aMessage.Read(offset, entry));
            mask |= entry.GetMask() & OPENTHREAD_CONFIG_PLATFORM_RADIO_PROPRIETARY_CHANNEL_MASK;
            break;
#endif
        }
        offset += entry.GetEntrySize();
    }

exit:
    return mask;
}

} // namespace MeshCoP
} // namespace ot
