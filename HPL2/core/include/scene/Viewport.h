/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include "engine/IUpdateEventLoop.h"
#include "engine/Event.h"
#include "engine/RTTI.h"
#include "graphics/Image.h"
#include "graphics/RenderTarget.h"
#include "graphics/RenderViewport.h"
#include "math/MathTypes.h"
#include "graphics/GraphicsTypes.h"
#include "gui/GuiTypes.h"
#include "scene/SceneTypes.h"
#include "windowing/NativeWindow.h"
#include <cstddef>
#include <cstdint>
#include <memory>

namespace hpl {

	//------------------------------------------

	class cScene;
	class cCamera;
	class iRenderer;
	class iFrameBuffer;
	class cRenderSettings;
	class cPostEffectComposite;
	class cWorld;
	class iViewportCallback;
	class iRendererCallback;
	class cGuiSet;

	//------------------------------------------

	class cViewport
	{
	public:

    	static constexpr size_t MaxViewportHandles = 32;
		using ResizeEvent = hpl::Event<hpl::cVector2l&>;
		using ViewportDisposeHandle = hpl::Event<>;

		cViewport(cScene *apScene);

		cViewport(cViewport&&);
		void operator=(cViewport&&);

		cViewport(const cViewport&) = delete;
		cViewport& operator=(const cViewport&) = delete;

		~cViewport();

		void SetActive(bool abX){ mbActive = abX;}
		void SetVisible(bool abX) { mbVisible = abX;}
		bool IsActive(){ return mbActive;}
		bool IsVisible(){ return mbVisible;}

		inline size_t GetHandle() { return m_handle; }

		void SetIsListener(bool abX){ mbIsListener = abX;}
		bool IsListener(){ return mbIsListener;}

		inline void SetCamera(cCamera *apCamera){ mpCamera = apCamera;}
		inline cCamera* GetCamera(){ return mpCamera;}

		void SetWorld(cWorld *apWorld);
		inline cWorld* GetWorld(){ return mpWorld;}

		void SetRenderer(iRenderer *apRenderer){ mpRenderer = apRenderer;}
		iRenderer* GetRenderer(){ return mpRenderer;}
		
		cRenderSettings* GetRenderSettings(){ return mpRenderSettings.get();}

		inline void SetPostEffectComposite(cPostEffectComposite *apPostEffectComposite) { 
			mpPostEffectComposite = apPostEffectComposite;
		}
		inline cPostEffectComposite* GetPostEffectComposite(){ return mpPostEffectComposite;}

		void AddGuiSet(cGuiSet *apSet);
		void RemoveGuiSet(cGuiSet *apSet);
		cGuiSetListIterator GetGuiSetIterator();

		// void SetPosition(const cVector2l& avPos){ mRenderTarget.setPosition(avPos);}
		inline void SetSize(const cVector2l& avSize){ 
			m_size = avSize;
			m_dirtyViewport = true;
		}

		// const cVector2l GetPosition(){ return mRenderTarget.GetPosition();}
		const cVector2l GetSize(){ return m_size;}

		void setImageDescriptor(const ImageDescriptor& imageDescriptor) { m_imageDescriptor = imageDescriptor; }
		
		void setRenderTarget(std::shared_ptr<RenderTarget> renderTarget) { m_renderTarget = renderTarget; }
		// if a render target is not set then return an empty render target
		// bgfx will draw to the back buffer in this case
		RenderTarget& GetRenderTarget() { 
			if(m_renderTarget) {
				return *m_renderTarget;
			}
			static RenderTarget emptyTarget = RenderTarget();
			return emptyTarget;
		}

		void setRenderViewport(RenderViewport renderTarget) { mRenderTarget = renderTarget; }
		RenderViewport& GetRenderViewport() { return mRenderTarget; }

		void bindToWindow(window::NativeWindowWrapper& window);
		void AddViewportCallback(iViewportCallback *apCallback);
		void RemoveViewportCallback(iViewportCallback *apCallback);
		void RunViewportCallbackMessage(eViewportMessage aMessage);

		void AddRendererCallback(iRendererCallback *apCallback);
		void RemoveRendererCallback(iRendererCallback *apCallback);
		tRendererCallbackList* GetRendererCallbackList(){ return &mlstRendererCallbacks;}


	private:
		bool m_dirtyViewport = false;
		ImageDescriptor m_imageDescriptor;
		
		cVector2l m_size = {0, 0};
		size_t m_handle = 0;
		std::shared_ptr<RenderTarget> m_renderTarget;
        IUpdateEventLoop::UpdateEvent::Handler m_updateEventHandler;
        window::WindowEvent::Handler m_windowEventHandler;
		ViewportDisposeHandle m_disposeEvent;

		cScene *mpScene;
		cCamera *mpCamera;
		cWorld *mpWorld;

        bool mbActive;
		bool mbVisible;
		bool mbIsListener;

		iRenderer *mpRenderer;
		cPostEffectComposite *mpPostEffectComposite;

		RenderViewport mRenderTarget;

		tViewportCallbackList mlstCallbacks;
		tRendererCallbackList mlstRendererCallbacks;
		tGuiSetList mlstGuiSets;

		std::unique_ptr<cRenderSettings> mpRenderSettings;
		
		template<typename TData>
		friend class UniqueViewportData;
	};


	// Data that is unique to a viewport
	template<typename TData>
    class UniqueViewportData final {
    public:
        UniqueViewportData()
            : m_createData([](cViewport& viewport) {
                return std::make_unique<TData>();
            }),
            m_dataValid([](cViewport& viewport, TData& target) {
                return true;
            }) {
        }
        UniqueViewportData(
            std::function<std::unique_ptr<TData>(cViewport&)>&& createHandler,
            std::function<bool(cViewport&, TData& payload)>&& dataValid = [](cViewport& viewport, TData& payload) { return true;})
            : m_createData(std::move(createHandler)),
            m_dataValid(std::move(dataValid)), 
            m_targets() {
        }

        UniqueViewportData(const UniqueViewportData& other) = delete;
        UniqueViewportData(UniqueViewportData&& other): 
            m_createData(std::move(other.m_createData)),
            m_dataValid(std::move(other.m_dataValid)),
			m_disposeHandlers(std::move(other.m_disposeHandlers)),
            m_targets(std::move(other.m_targets)) {

        }
        UniqueViewportData& operator=(const UniqueViewportData& other) = delete;
        void operator=(UniqueViewportData&& other) {
            m_createData = std::move(other.m_createData);
            m_dataValid = std::move(other.m_dataValid);
			m_disposeHandlers = std::move(other.m_disposeHandlers);
            m_targets = std::move(other.m_targets);
        }

        TData& resolve(cViewport& viewport) {
			uint8_t handle = viewport.GetHandle();
			BX_ASSERT(handle < cViewport::MaxViewportHandles, "Invalid viewport handle")
            auto& target = m_targets[handle];
            if (!target || !m_dataValid(viewport, *target)) {
                target = m_createData(viewport);
				auto& disposeHandle = m_disposeHandlers[handle];
				if(!disposeHandle.IsConnected()) {
					disposeHandle = std::move(cViewport::ViewportDisposeHandle::Handler([&, handle]() {
						m_targets[handle] = nullptr;
					}));
					disposeHandle.Connect(viewport.m_disposeEvent);
				}
            }
			BX_ASSERT(target, "Failed to create viewport data");
            return *target;
		}
    private:
	
		std::function<std::unique_ptr<TData>(cViewport&)> m_createData;
        std::function<bool(cViewport&, TData& target)> m_dataValid;
        std::array<cViewport::ViewportDisposeHandle::Handler, cViewport::MaxViewportHandles> m_disposeHandlers;
        std::array<std::unique_ptr<TData>, cViewport::MaxViewportHandles> m_targets;

    };

};
