/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/FMRadio.h"
#include "nsContentUtils.h"
#include "mozilla/Hal.h"
#include "mozilla/HalTypes.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/FMRadioBinding.h"
#include "mozilla/dom/ContentChild.h"
#include "mozilla/dom/PFMRadioChild.h"
#include "mozilla/dom/FMRadioService.h"
#include "DOMRequest.h"
#include "nsDOMClassInfo.h"
#include "nsIDocShell.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIAudioManager.h"

#undef LOG
#define LOG(args...) FM_LOG("FMRadio", args)

// The pref indicates if the device has an internal antenna.
// If the pref is true, the antanna will be always available.
#define DOM_FM_ANTENNA_INTERNAL_PREF "dom.fmradio.antenna.internal"

using namespace mozilla::hal;
using mozilla::Preferences;

BEGIN_FMRADIO_NAMESPACE

class FMRadioRequest MOZ_FINAL : public FMRadioReplyRunnable
                               , public DOMRequest
{
public:
  NS_DECL_ISUPPORTS_INHERITED

  FMRadioRequest(nsPIDOMWindow* aWindow, FMRadio* aFMRadio)
    : DOMRequest(aWindow)
  {
    // |FMRadio| inherits from |nsIDOMEventTarget| and |nsISupportsWeakReference|
    // which both inherits from nsISupports, so |nsISupports| is an ambiguous
    // base of |FMRadio|, we have to cast |aFMRadio| to one of the base classes.
    mFMRadio = do_GetWeakReference(static_cast<nsIDOMEventTarget*>(aFMRadio));
  }

  ~FMRadioRequest() { }

  NS_IMETHODIMP
  Run()
  {
    MOZ_ASSERT(NS_IsMainThread(), "Wrong thread!");

    nsCOMPtr<nsIDOMEventTarget> target = do_QueryReferent(mFMRadio);
    if (!target) {
      return NS_OK;
    }

    FMRadio* fmRadio = static_cast<FMRadio*>(
      static_cast<nsIDOMEventTarget*>(target));

    if (fmRadio->mIsShutdown) {
      return NS_OK;
    }

    switch (mResponseType.type()) {
      case FMRadioResponseType::TErrorResponse:
        FireError(mResponseType.get_ErrorResponse().error());
        break;
      case FMRadioResponseType::TSuccessResponse:
        FireSuccess(JS::UndefinedHandleValue);
        break;
      default:
        MOZ_CRASH();
    }

    return NS_OK;
  }

private:
  nsWeakPtr mFMRadio;
};

NS_IMPL_ISUPPORTS_INHERITED0(FMRadioRequest, DOMRequest)

FMRadio::FMRadio()
  : mHeadphoneState(SWITCH_STATE_OFF)
  , mHasInternalAntenna(false)
  , mIsShutdown(false)
{
  LOG("FMRadio is initialized.");

  SetIsDOMBinding();
}

FMRadio::~FMRadio()
{
}

void
FMRadio::Init(nsPIDOMWindow *aWindow)
{
  BindToOwner(aWindow);

  IFMRadioService::Singleton()->AddObserver(this);

  mHasInternalAntenna = Preferences::GetBool(DOM_FM_ANTENNA_INTERNAL_PREF,
                                             /* default = */ false);
  if (mHasInternalAntenna) {
    LOG("We have an internal antenna.");
  } else {
    mHeadphoneState = GetCurrentSwitchState(SWITCH_HEADPHONES);
    RegisterSwitchObserver(SWITCH_HEADPHONES, this);
  }

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);
  target->AddSystemEventListener(NS_LITERAL_STRING("visibilitychange"), this,
                                 /* useCapture = */ true,
                                 /* wantsUntrusted = */ false);

  mAudioChannelAgent = do_CreateInstance("@mozilla.org/audiochannelagent;1");
  if (!mAudioChannelAgent) {
    return;
  }

  mAudioChannelAgent->InitWithWeakCallback(nsIAudioChannelAgent::AUDIO_AGENT_CHANNEL_CONTENT,
                                           this);

  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  if (docshell) {
    bool isActive = false;
    docshell->GetIsActive(&isActive);
    mAudioChannelAgent->SetVisibilityState(isActive);
  }
}

void
FMRadio::Shutdown()
{
  IFMRadioService::Singleton()->RemoveObserver(this);

  if (!mHasInternalAntenna) {
    UnregisterSwitchObserver(SWITCH_HEADPHONES, this);
  }

  nsCOMPtr<nsIDOMEventTarget> target = do_QueryInterface(GetOwner());
  NS_ENSURE_TRUE_VOID(target);
  target->RemoveSystemEventListener(NS_LITERAL_STRING("visibilitychange"), this,
                                    /* useCapture = */ true);

  mIsShutdown = true;
}

JSObject*
FMRadio::WrapObject(JSContext* aCx, JS::Handle<JSObject*> aScope)
{
  return FMRadioBinding::Wrap(aCx, aScope, this);
}

void
FMRadio::Notify(const SwitchEvent& aEvent)
{
  MOZ_ASSERT(!mHasInternalAntenna);

  if (mHeadphoneState != aEvent.status()) {
    mHeadphoneState = aEvent.status();

    DispatchTrustedEvent(NS_LITERAL_STRING("antennaavailablechange"));
  }
}

void
FMRadio::Notify(const FMRadioEventType& aType)
{
  switch (aType) {
    case FrequencyChanged:
      DispatchTrustedEvent(NS_LITERAL_STRING("frequencychange"));
      break;
    case EnabledChanged:
      if (Enabled()) {
        int32_t playingState = 0;
        mAudioChannelAgent->StartPlaying(&playingState);
        SetCanPlay(playingState == AudioChannelState::AUDIO_CHANNEL_STATE_NORMAL);

        DispatchTrustedEvent(NS_LITERAL_STRING("enabled"));
      } else {
        mAudioChannelAgent->StopPlaying();

        DispatchTrustedEvent(NS_LITERAL_STRING("disabled"));
      }
      break;
    default:
      MOZ_CRASH();
  }
}

/* static */
bool
FMRadio::Enabled()
{
  return IFMRadioService::Singleton()->IsEnabled();
}

bool
FMRadio::AntennaAvailable() const
{
  return mHasInternalAntenna ? true : mHeadphoneState != SWITCH_STATE_OFF;
}

Nullable<double>
FMRadio::GetFrequency() const
{
  return Enabled() ?
    Nullable<double>(IFMRadioService::Singleton()->GetFrequency()) :
    Nullable<double>();
}

double
FMRadio::FrequencyUpperBound() const
{
  return IFMRadioService::Singleton()->GetFrequencyUpperBound();
}

double
FMRadio::FrequencyLowerBound() const
{
  return IFMRadioService::Singleton()->GetFrequencyLowerBound();
}

double
FMRadio::ChannelWidth() const
{
  return IFMRadioService::Singleton()->GetChannelWidth();
}

already_AddRefed<DOMRequest>
FMRadio::Enable(double aFrequency)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->Enable(aFrequency, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::Disable()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->Disable(r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SetFrequency(double aFrequency)
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->SetFrequency(aFrequency, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SeekUp()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->Seek(FM_RADIO_SEEK_DIRECTION_UP, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::SeekDown()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->Seek(FM_RADIO_SEEK_DIRECTION_DOWN, r);

  return r.forget();
}

already_AddRefed<DOMRequest>
FMRadio::CancelSeek()
{
  nsCOMPtr<nsPIDOMWindow> win = GetOwner();
  if (!win) {
    return nullptr;
  }

  nsRefPtr<FMRadioRequest> r = new FMRadioRequest(win, this);
  IFMRadioService::Singleton()->CancelSeek(r);

  return r.forget();
}

NS_IMETHODIMP
FMRadio::HandleEvent(nsIDOMEvent* aEvent)
{
  nsAutoString type;
  aEvent->GetType(type);

  if (!type.EqualsLiteral("visibilitychange")) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(GetOwner());
  NS_ENSURE_TRUE(docshell, NS_ERROR_FAILURE);

  bool isActive = false;
  docshell->GetIsActive(&isActive);

  mAudioChannelAgent->SetVisibilityState(isActive);
  return NS_OK;
}

NS_IMETHODIMP
FMRadio::CanPlayChanged(int32_t aCanPlay)
{
  SetCanPlay(aCanPlay == AudioChannelState::AUDIO_CHANNEL_STATE_NORMAL);
  return NS_OK;
}

void
FMRadio::SetCanPlay(bool aCanPlay)
{
  IFMRadioService::Singleton()->EnableAudio(aCanPlay);
}

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION_INHERITED(FMRadio)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
  NS_INTERFACE_MAP_ENTRY(nsIAudioChannelAgentCallback)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEventListener)
NS_INTERFACE_MAP_END_INHERITING(nsDOMEventTargetHelper)

NS_IMPL_ADDREF_INHERITED(FMRadio, nsDOMEventTargetHelper)
NS_IMPL_RELEASE_INHERITED(FMRadio, nsDOMEventTargetHelper)

END_FMRADIO_NAMESPACE

