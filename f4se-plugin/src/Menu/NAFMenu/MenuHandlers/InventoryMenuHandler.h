#pragma once
#include "Menu/BindableMenu.h"
#include "Misc/MathUtil.h"
#include "Scene/SceneBase.h"
#include "Scene/SceneManager.h"
#include "RE/Bethesda/UserEvents.h"

namespace Menu::NAF
{
	class InventoryMenuHandler : public BindableMenu<InventoryMenuHandler, SUB_MENU_TYPE::kInventories>
	{
	public:
		class MenuOpenCloseEventHandler :
			public RE::BSTEventSink<RE::MenuOpenCloseEvent>,
			public Singleton<MenuOpenCloseEventHandler>,
			public Data::EventListener<MenuOpenCloseEventHandler>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent& event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
			{
				if (event.menuName != "ContainerMenu")
					return RE::BSEventNotifyControl::kContinue;

				auto state = Menu::PersistentMenuState::GetSingleton();
				if (state->restoreSubmenu != Menu::SUB_MENU_TYPE::kInventories && state->restoreSubmenu != Menu::SUB_MENU_TYPE::kManageScenes)
					return RE::BSEventNotifyControl::kContinue;

				if (event.opening) {
					RE::UI::GetSingleton()->UpdateMenuMode("ContainerMenu", false);
				} else {
					if (auto& a = state->sceneData->pendingActor; a.has_value()) {
						auto targetActor = a.value().get().get();
						if (targetActor) {
							targetActor->ModifyKeyword(Data::Forms::ShowWornItemsKW, false);
							a = std::nullopt;
						}
					}

					if (state->reenablePlayerControls)
					{
						// unlock mouse wheel
						RE::PlayerCharacter::GetSingleton()->ReenableInputForPlayer();
						state->reenablePlayerControls = false;
					}

					F4SE::GetTaskInterface()->AddTask([]() {
						RE::UIMessageQueue::GetSingleton()->AddMessage("NAFMenu", RE::UI_MESSAGE_TYPE::kShow);
					});
				}

				return RE::BSEventNotifyControl::kContinue;
			}

			void OnGameDataReady(Data::Events::event_type, Data::Events::EventData&)
			{
				RE::UI::GetSingleton()->RegisterSink(this);
			}

		protected:
			friend class Singleton;
			MenuOpenCloseEventHandler()
			{
				RegisterListener(Data::Events::GAME_DATA_READY, &MenuOpenCloseEventHandler::OnGameDataReady);
			}
		};

		using BindableMenu::BindableMenu;
		virtual ~InventoryMenuHandler() {}

		uint64_t curSceneId = 0;

		virtual void InitSubmenu() override
		{
			BindableMenu::InitSubmenu();

			auto sceneData = PersistentMenuState::SceneData::GetSingleton();
			curSceneId = sceneData->pendingSceneId;
			sceneData->pendingSceneId = 0;
		}

		virtual BindingsVector GetBindings() override
		{
			manager->SetMenuTitle("Actor Inventories");

			BindingsVector result;
			Scene::SceneManager::VisitScene(curSceneId, [&](Scene::IScene* scn) {
				scn->ForEachActor([&](RE::Actor* currentActor, Scene::ActorPropertyMap&) {
					result.push_back({ GameUtil::GetActorName(currentActor),
						Bind(&InventoryMenuHandler::ShowActorInventory, currentActor->GetActorHandle()) });
				});
			});

			return result;
		}

		void ShowActorInventory(RE::ActorHandle h, int)
		{
			RE::Actor* a = h.get().get();
			if (a)
			{
				bool hadSWIKeyword = a->HasKeyword(Data::Forms::ShowWornItemsKW);
				if (!hadSWIKeyword)
				{
					a->ModifyKeyword(Data::Forms::ShowWornItemsKW, true);
				}

				auto state = PersistentMenuState::GetSingleton();
				state->sceneData->pendingSceneId = curSceneId;
				state->restoreSubmenu = kInventories;							
				if (!hadSWIKeyword) {
					state->sceneData->pendingActor = h;
				} else {
					state->sceneData->pendingActor = std::nullopt;
				}

				manager->CloseMenu();				

				// lock mouse wheel
				F4SE::stl::enumeration<RE::UserEvents::USER_EVENT_FLAG, uint32_t> flags;
				flags.set(RE::UserEvents::USER_EVENT_FLAG::kLooking);
				flags.set(RE::UserEvents::USER_EVENT_FLAG::kPOVSwitch);
				RE::PlayerCharacter::GetSingleton()->DisableInputForPlayer("MuhLayer", flags.underlying());
				state->reenablePlayerControls = true;

				// open actor container menu
				RE::OpenContainerMenu(a, 3, false);
				
			}
		}

		virtual void BackImpl() override
		{
			manager->GotoMenu(kManageScenes, true);
		}
	};
}
