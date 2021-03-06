/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SmsMessage.h"
#include "SmsSegmentInfo.h"
#include "SmsService.h"

namespace mozilla {
namespace dom {
namespace mobilemessage {

NS_IMPL_ISUPPORTS1(SmsService, nsISmsService)

NS_IMETHODIMP
SmsService::GetSmsDefaultServiceId(uint32_t* aServiceId)
{
  NS_ERROR("We should not be here!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SmsService::HasSupport(bool* aHasSupport)
{
  *aHasSupport = false;
  return NS_OK;
}

NS_IMETHODIMP
SmsService::GetSegmentInfoForText(const nsAString& aText,
                                  nsIMobileMessageCallback* aRequest)
{
  NS_ERROR("We should not be here!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SmsService::Send(uint32_t         aServiceId,
                 const nsAString& aNumber,
                 const nsAString& aMessage,
                 const bool       aSilent,
                 nsIMobileMessageCallback* aRequest)
{
  NS_ERROR("We should not be here!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SmsService::IsSilentNumber(const nsAString& aNumber,
                           bool*            aIsSilent)
{
  NS_ERROR("We should not be here!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SmsService::AddSilentNumber(const nsAString& aNumber)
{
  NS_ERROR("We should not be here!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SmsService::RemoveSilentNumber(const nsAString& aNumber)
{
  NS_ERROR("We should not be here!");
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
SmsService::GetSmscAddress(uint32_t aServiceId,
                           nsIMobileMessageCallback *aRequest)
{
  NS_ERROR("We should not be here!");
  return NS_OK;
}

} // namespace mobilemessage
} // namespace dom
} // namespace mozilla
