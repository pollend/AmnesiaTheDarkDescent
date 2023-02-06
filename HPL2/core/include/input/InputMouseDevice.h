#pragma once

#include <math/MathTypes.h>
#include <input/InputChannel.h>
#include <math/Uuid.h>
#include <engine/RTTI.h>
#include <input/InputChannel.h>
#include <memory>

namespace hpl::input {

    enum class MouseButton {
		Left,
		Middle,
		Right,
		WheelUp,
		WheelDown,
		Button6,
		Button7,
		Button8,
		Button9
	};

	// opaque handle to the internal mouse state
	struct InternalMouseHandle {
    };

    class InputMouseDevice : public InputChannel {
        HPL_RTTI_IMPL_CLASS(InputChannel, InputMouseDevice, "{ac1b28f3-7a0f-4442-96bb-99b64adb5be6}")
    public:

        InputMouseDevice(hpl::Guid deviceUID)
            : InputChannel(DeviceType::Mouse, deviceUID) {
        }
        
		class Implementation {
		public:
			virtual ~Implementation() = default;
			virtual bool ButtonIsDown(MouseButton)=0;
			virtual cVector2l GetAbsPosition()=0;
			virtual cVector2l GetRelPosition()=0;
		};
        static Implementation* CreateInputDevice(void* handle); 

		~InputMouseDevice() = default;
		
	    
        /**
		 * Check if a mouse button is down
		 * \param eMouseButton the button to check
		 * \return
		 */
		virtual bool ButtonIsDown(MouseButton)=0;
        /**
		 * Get the absolute pos of the mouse.
		 * \return
		 */
		virtual cVector2l GetAbsPosition()=0;
		/**
		 * Get the relative movement.
		 * \return
		 */
		virtual cVector2l GetRelPosition()=0;
		/**
		 * \param eMouseButton The button to change to string.
		 * \return The name of the button as a string.
		 */
		virtual tString ButtonToString(MouseButton);
		/**
		 * \param tString Name of the button
		 * \return enum of the button.
		 */
		virtual MouseButton StringToButton(const tString&);
	private:
		std::unique_ptr<Implementation> m_impl;
    };
} // namespace hpl::input