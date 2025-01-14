/*  SPDX-License-Identifier: GPL-2.0-or-later */
/*!********************************************************************

  Audacity: A Digital Audio Editor

  RealtimeEffectStateUI.cpp

  Dmitry Vedenko

**********************************************************************/
#include "RealtimeEffectStateUI.h"

#include <cassert>

#include "EffectUI.h"
#include "RealtimeEffectState.h"

#include "EffectManager.h"
#include "ProjectWindow.h"
#include "Track.h"

namespace
{
const RealtimeEffectState::RegisteredFactory realtimeEffectStateUIFactory { [](auto& state)
   { return std::make_unique<RealtimeEffectStateUI>(state); }
};
} // namespace


RealtimeEffectStateUI::RealtimeEffectStateUI(RealtimeEffectState& state)
    : mRealtimeEffectState(state)
{
}

RealtimeEffectStateUI::~RealtimeEffectStateUI()
{
   Hide();
}

bool RealtimeEffectStateUI::IsShown() const noexcept
{
   return mEffectUIHost != nullptr;
}

void RealtimeEffectStateUI::Show(AudacityProject& project)
{
   if (mEffectUIHost != nullptr && mEffectUIHost->IsShown())
   {
      // Bring the host to front
      mEffectUIHost->Raise();
      return;
   }

   const auto ID = mRealtimeEffectState.GetID();
   const auto effectPlugin = EffectManager::Get().GetEffect(ID);

   if (effectPlugin == nullptr)
      return;

   EffectUIClientInterface* client = effectPlugin->GetEffectUIClientInterface();

   // Effect has no client interface
   if (client == nullptr)
      return;

   auto& projectWindow = ProjectWindow::Get(project);

   std::shared_ptr<EffectInstance> pInstance;

   auto access = mRealtimeEffectState.GetAccess();

   // EffectUIHost invokes shared_from_this on access
   Destroy_ptr<EffectUIHost> dlg(safenew EffectUIHost(
      &projectWindow, project, *effectPlugin, *client, pInstance, *access,
      mRealtimeEffectState.shared_from_this()));

   if (!dlg->Initialize())
      return;

   // Dialog is owned by its parent now!
   mEffectUIHost = dlg.release();

   UpdateTitle();

   client->ShowClientInterface(
      projectWindow, *mEffectUIHost, mEffectUIHost->GetValidator(), false);

   // The dialog was modal? That shouldn't have happened
   if (mEffectUIHost == nullptr || !mEffectUIHost->IsShown())
   {
      assert(false);
      mEffectUIHost = {};
   }

   mProjectWindowDestroyedSubscription = projectWindow.Subscribe(
      [this](ProjectWindowDestroyedMessage) { Hide(); });
}

void RealtimeEffectStateUI::Hide()
{
   if (mEffectUIHost != nullptr)
   {
      // EffectUIHost calls Destroy in OnClose handler
      mEffectUIHost->Close();
      mEffectUIHost = {};
   }
}

void RealtimeEffectStateUI::Toggle(AudacityProject& project)
{
   if(IsShown())
      Hide();
   else
      Show(project);
}

void RealtimeEffectStateUI::UpdateTrackData(const Track& track)
{
   mTrackName = track.GetName();

   UpdateTitle();
}

RealtimeEffectStateUI& RealtimeEffectStateUI::Get(RealtimeEffectState& state)
{      
   return state.Get<RealtimeEffectStateUI&>(realtimeEffectStateUIFactory);
}

const RealtimeEffectStateUI&
RealtimeEffectStateUI::Get(const RealtimeEffectState& state)
{
   return Get(const_cast<RealtimeEffectState&>(state));
}

void RealtimeEffectStateUI::UpdateTitle()
{
   if (mEffectUIHost == nullptr)
      return;

   if (mEffectName.empty())
   {
      const auto ID = mRealtimeEffectState.GetID();
      const auto effectPlugin = EffectManager::Get().GetEffect(ID);

      if (effectPlugin != nullptr)
         mEffectName = effectPlugin->GetDefinition().GetName();
   }

   const auto title =
      mTrackName.empty() ?
         mEffectName :
         /* i18n-hint: First %s is an effect name, second is a track name */
         XO("%s - %s").Format(mEffectName, mTrackName);

   mEffectUIHost->SetTitle(title);
   mEffectUIHost->SetName(title);
}

