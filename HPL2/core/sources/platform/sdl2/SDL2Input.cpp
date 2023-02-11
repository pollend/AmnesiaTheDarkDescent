#include <input/InputMouseDevice.h>

#include <engine/IUpdateEventLoop.h>

#include "absl/container/inlined_vector.h"
#include "engine/Interface.h"
#include "input/InputTypes.h"
#include "math/MathTypes.h"
#include "windowing/NativeWindow.h"
#include <memory>

#include <input/InputKeyboardDevice.h>
#include <input/InputMouseDevice.h>

#include <SDL2/SDL_events.h>
#include <SDL2/SDL_mouse.h>
#include <mutex>
#include <queue>
#include <vector>

namespace hpl::input::internal {

    namespace mouse {
        struct InternalInputMouseImpl {
            window::internal::WindowInternalEvent::Handler m_windowEventHandle;
            cVector2l m_mousePosition;
            cVector2l m_mouseRelPosition;
            std::array<bool, static_cast<size_t>(MouseButton::LastEnum)> m_mouseButton;
        };

        InternalInputMouseHandle Initialize() {
            InternalInputMouseHandle handle =
                InternalInputMouseHandle{ InternalInputMouseHandle::Ptr(new InternalInputMouseImpl(), [](void* ptr) {
                    auto impl = static_cast<InternalInputMouseImpl*>(ptr);
                    impl->m_windowEventHandle.Disconnect();
                    delete impl;
                }) };

            auto* impl = static_cast<InternalInputMouseImpl*>(handle.Get());
            impl->m_windowEventHandle = window::internal::WindowInternalEvent::Handler([impl](hpl::window::InternalEvent& event) {
                auto& sdlEvent = *event.m_sdlEvent;

                switch (sdlEvent.type) {
                case SDL_EventType::SDL_MOUSEMOTION:
                    impl->m_mousePosition = cVector2l(sdlEvent.motion.x, sdlEvent.motion.y);
                    impl->m_mouseRelPosition = cVector2l(sdlEvent.motion.xrel, sdlEvent.motion.yrel);
                    break;
                case SDL_EventType::SDL_MOUSEBUTTONDOWN:
                case SDL_EventType::SDL_MOUSEBUTTONUP:
                    {
                        const bool isPressed = sdlEvent.button.state == SDL_PRESSED;
                        switch (sdlEvent.button.button) {
                        case SDL_BUTTON_LEFT:
                            impl->m_mouseButton[static_cast<size_t>(MouseButton::Left)] = isPressed;
                            break;
                        case SDL_BUTTON_MIDDLE:
                            impl->m_mouseButton[static_cast<size_t>(MouseButton::Middle)] = isPressed;
                            break;
                        case SDL_BUTTON_RIGHT:
                            impl->m_mouseButton[static_cast<size_t>(MouseButton::Right)] = isPressed;
                            break;
                        case SDL_BUTTON_X1:
                            impl->m_mouseButton[static_cast<size_t>(MouseButton::Button6)] = isPressed;
                            break;
                        case SDL_BUTTON_X2:
                            impl->m_mouseButton[static_cast<size_t>(MouseButton::Button7)] = isPressed;
                            break;
                        }
                        break;
                    }
                case SDL_EventType::SDL_MOUSEWHEEL:
                    if (sdlEvent.wheel.y > 0) {
                        impl->m_mouseButton[static_cast<size_t>(MouseButton::WheelUp)] = true;
                        impl->m_mouseButton[static_cast<size_t>(MouseButton::WheelDown)] = false;
                    } else if (sdlEvent.wheel.y < 0) {
                        impl->m_mouseButton[static_cast<size_t>(MouseButton::WheelUp)] = false;
                        impl->m_mouseButton[static_cast<size_t>(MouseButton::WheelDown)] = true;
                    }
                    break;
                default:
                    break;
                }
            });

            return handle;
        }
        cVector2l GetAbsPosition(InternalInputMouseHandle& handle) {
            auto* impl = static_cast<InternalInputMouseImpl*>(handle.Get());
            return impl->m_mousePosition;
        }

        cVector2l GetRelPosition(InternalInputMouseHandle& handle) {
            auto* impl = static_cast<InternalInputMouseImpl*>(handle.Get());
            return impl->m_mouseRelPosition;
        }

        window::internal::WindowInternalEvent::Handler& GetWindowEventHandle(InternalInputMouseHandle& handle) {
            auto* impl = static_cast<InternalInputMouseImpl*>(handle.Get());
            return impl->m_windowEventHandle;
        }
    } // namespace mouse

    namespace keyboard {
        struct InternalInputKeyboardImpl {
            window::internal::WindowInternalEvent::Handler m_windowEventHandle;
            IUpdateEventLoop::UpdateEvent::Handler m_postUpdate;
            IUpdateEventLoop::UpdateEvent::Handler m_preUpdate;
            std::mutex m_mutex;
            absl::InlinedVector<char, 256> m_queuedCharacter;
            absl::InlinedVector<char, 256> m_stagingCharacter;
            absl::InlinedVector<KeyPress, 256> m_queuedPresses;
            absl::InlinedVector<KeyPress, 256> m_stagingPresses;
            std::array<bool, static_cast<size_t>(Key::LastEnum)> m_keyPressState = {{0}};
        };

        static Key SDLToKey(uint32_t key) {
            switch (key) {
            case SDLK_BACKSPACE:
                return Key::KeyBackSpace;
            case SDLK_TAB:
                return Key::KeyTab;
            case SDLK_CLEAR:
                return Key::KeyClear;
            case SDLK_RETURN:
                return Key::KeyReturn;
            case SDLK_PAUSE:
                return Key::KeyPause;
            case SDLK_ESCAPE:
                return Key::KeyEscape;
            case SDLK_SPACE:
                return Key::KeySpace;
            case SDLK_EXCLAIM:
                return Key::KeyExclaim;
            case SDLK_QUOTEDBL:
                return Key::KeyQuoteDouble;
            case SDLK_HASH:
                return Key::KeyHash;
            case SDLK_DOLLAR:
                return Key::KeyDollar;
            case SDLK_AMPERSAND:
                return Key::KeyAmpersand;
            case SDLK_QUOTE:
                return Key::KeyQuote;
            case SDLK_LEFTPAREN:
                return Key::KeyLeftParen;
            case SDLK_RIGHTPAREN:
                return Key::KeyRightParen;
            case SDLK_ASTERISK:
                return Key::KeyAsterisk;
            case SDLK_PLUS:
                return Key::KeyPlus;
            case SDLK_COMMA:
                return Key::KeyComma;
            case SDLK_MINUS:
                return Key::KeyMinus;
            case SDLK_PERIOD:
                return Key::KeyPeriod;
            case SDLK_SLASH:
                return Key::KeySlash;
            case SDLK_0:
                return Key::Key0;
            case SDLK_1:
                return Key::Key1;
            case SDLK_2:
                return Key::Key2;
            case SDLK_3:
                return Key::Key3;
            case SDLK_4:
                return Key::Key4;
            case SDLK_5:
                return Key::Key5;
            case SDLK_6:
                return Key::Key6;
            case SDLK_7:
                return Key::Key7;
            case SDLK_8:
                return Key::Key8;
            case SDLK_9:
                return Key::Key9;
            case SDLK_COLON:
                return Key::KeyColon;
            case SDLK_SEMICOLON:
                return Key::KeySemiColon;
            case SDLK_LESS:
                return Key::KeyLess;
            case SDLK_EQUALS:
                return Key::KeyEquals;
            case SDLK_GREATER:
                return Key::KeyGreater;
            case SDLK_QUESTION:
                return Key::KeyQuestion;
            case SDLK_AT:
                return Key::KeyAt;
            case SDLK_LEFTBRACKET:
                return Key::KeyLeftBracket;
            case SDLK_BACKSLASH:
                return Key::KeyBackSlash;
            case SDLK_RIGHTBRACKET:
                return Key::KeyRightBracket;
            case SDLK_CARET:
                return Key::KeyCaret;
            case SDLK_UNDERSCORE:
                return Key::KeyUnderscore;
            case SDLK_BACKQUOTE:
                return Key::KeyBackSlash;
            case SDLK_a:
                return Key::KeyA;
            case SDLK_b:
                return Key::KeyB;
            case SDLK_c:
                return Key::KeyC;
            case SDLK_d:
                return Key::KeyD;
            case SDLK_e:
                return Key::KeyE;
            case SDLK_f:
                return Key::KeyF;
            case SDLK_g:
                return Key::KeyG;
            case SDLK_h:
                return Key::KeyH;
            case SDLK_i:
                return Key::KeyI;
            case SDLK_j:
                return Key::KeyJ;
            case SDLK_k:
                return Key::KeyK;
            case SDLK_l:
                return Key::KeyL;
            case SDLK_m:
                return Key::KeyM;
            case SDLK_n:
                return Key::KeyN;
            case SDLK_o:
                return Key::KeyO;
            case SDLK_p:
                return Key::KeyP;
            case SDLK_q:
                return Key::KeyQ;
            case SDLK_r:
                return Key::KeyR;
            case SDLK_s:
                return Key::KeyS;
            case SDLK_t:
                return Key::KeyT;
            case SDLK_u:
                return Key::KeyU;
            case SDLK_v:
                return Key::KeyV;
            case SDLK_w:
                return Key::KeyW;
            case SDLK_x:
                return Key::KeyX;
            case SDLK_y:
                return Key::KeyY;
            case SDLK_z:
                return Key::KeyZ;
            case SDLK_DELETE:
                return Key::KeyDelete;
            case SDLK_KP_0:
                return Key::KeyKP_0;
            case SDLK_KP_1:
                return Key::KeyKP_1;
            case SDLK_KP_2:
                return Key::KeyKP_2;
            case SDLK_KP_3:
                return Key::KeyKP_3;
            case SDLK_KP_4:
                return Key::KeyKP_4;
            case SDLK_KP_5:
                return Key::KeyKP_5;
            case SDLK_KP_6:
                return Key::KeyKP_6;
            case SDLK_KP_7:
                return Key::KeyKP_7;
            case SDLK_KP_8:
                return Key::KeyKP_8;
            case SDLK_KP_9:
                return Key::KeyKP_9;
            case SDLK_KP_PERIOD:
                return Key::KeyKP_Period;
            case SDLK_KP_DIVIDE:
                return Key::KeyKP_Divide;
            case SDLK_KP_MULTIPLY:
                return Key::KeyKP_Multiply;
            case SDLK_KP_MINUS:
                return Key::KeyKP_Minus;
            case SDLK_KP_PLUS:
                return Key::KeyKP_Plus;
            case SDLK_KP_ENTER:
                return Key::KeyKP_Enter;
            case SDLK_KP_EQUALS:
                return Key::KeyKP_Equals;
            case SDLK_UP:
                return Key::KeyUp;
            case SDLK_DOWN:
                return Key::KeyDown;
            case SDLK_RIGHT:
                return Key::KeyRight;
            case SDLK_LEFT:
                return Key::KeyLeft;
            case SDLK_INSERT:
                return Key::KeyInsert;
            case SDLK_HOME:
                return Key::KeyHome;
            case SDLK_END:
                return Key::KeyEnd;
            case SDLK_PAGEUP:
                return Key::KeyPageUp;
            case SDLK_PAGEDOWN:
                return Key::KeyPageDown;
            case SDLK_F1:
                return Key::KeyF1;
            case SDLK_F2:
                return Key::KeyF2;
            case SDLK_F3:
                return Key::KeyF3;
            case SDLK_F4:
                return Key::KeyF4;
            case SDLK_F5:
                return Key::KeyF5;
            case SDLK_F6:
                return Key::KeyF6;
            case SDLK_F7:
                return Key::KeyF7;
            case SDLK_F8:
                return Key::KeyF8;
            case SDLK_F9:
                return Key::KeyF9;
            case SDLK_F10:
                return Key::KeyF10;
            case SDLK_F11:
                return Key::KeyF11;
            case SDLK_F12:
                return Key::KeyF12;
            case SDLK_F13:
                return Key::KeyF13;
            case SDLK_F14:
                return Key::KeyF14;
            case SDLK_F15:
                return Key::KeyF15;
            case SDLK_NUMLOCKCLEAR:
                return Key::KeyNumLock;
            case SDLK_SCROLLLOCK:
                return Key::KeyScrollLock;
            case SDLK_LGUI:
                return Key::KeyLeftSuper;
            case SDLK_RGUI:
                return Key::KeyRightSuper;
            case SDLK_PRINTSCREEN:
                return Key::KeyPrint;
            case SDLK_CAPSLOCK:
                return Key::KeyCapsLock;
            case SDLK_RSHIFT:
                return Key::KeyRightShift;
            case SDLK_LSHIFT:
                return Key::KeyLeftShift;
            case SDLK_RCTRL:
                return Key::KeyRightCtrl;
            case SDLK_LCTRL:
                return Key::KeyLeftCtrl;
            case SDLK_RALT:
                return Key::KeyRightAlt;
            case SDLK_LALT:
                return Key::KeyLeftAlt;
            case SDLK_MODE:
                return Key::KeyMode;
            case SDLK_HELP:
                return Key::KeyHelp;
            case SDLK_SYSREQ:
                return Key::KeySysReq;
            case SDLK_MENU:
                return Key::KeyMenu;
            case SDLK_POWER:
                return Key::KeyPower;
            }
            return Key::KeyEmpty;
        };

        InternalInputKeyboardHandle Initialize() {
            InternalInputKeyboardHandle handle =
                InternalInputKeyboardHandle{ InternalInputKeyboardHandle::Ptr(new InternalInputKeyboardImpl(), [](void* ptr) {
                    auto impl = static_cast<InternalInputKeyboardImpl*>(ptr);
                    impl->m_windowEventHandle.Disconnect();
                    delete impl;
                }) };
            auto* impl = static_cast<InternalInputKeyboardImpl*>(handle.Get());

            impl->m_postUpdate = IUpdateEventLoop::UpdateEvent::Handler([impl]() {
                // we've processed the presses, so clear them
                impl->m_stagingPresses.clear();
                impl->m_stagingCharacter.clear();
            });
            impl->m_preUpdate = IUpdateEventLoop::UpdateEvent::Handler([impl]() {
                // move queued presses to staging for processing by the main thread
                std::lock_guard<std::mutex> lock(impl->m_mutex);
                for(auto itKey = impl->m_queuedPresses.rbegin(); itKey != impl->m_queuedPresses.rend(); ++itKey) {
                    impl->m_stagingPresses.push_back(*itKey);
                }
                for(auto itKey = impl->m_queuedCharacter.rbegin(); itKey != impl->m_queuedCharacter.rend(); ++itKey) {
                    impl->m_stagingCharacter.push_back(*itKey);
                }
                impl->m_queuedCharacter.clear();
                impl->m_queuedPresses.clear();
            });

            if (auto updateLoop = Interface<IUpdateEventLoop>::Get()) {
                updateLoop->Subscribe(BrodcastEvent::PostUpdate, impl->m_postUpdate);
                updateLoop->Subscribe(BrodcastEvent::PreUpdate, impl->m_preUpdate);
            } else {
                BX_ASSERT(false, "No update loop found")
            }

            impl->m_windowEventHandle = window::internal::WindowInternalEvent::Handler([impl](hpl::window::InternalEvent& event) {
                auto& sdlEvent = *event.m_sdlEvent;
                switch(sdlEvent.type) {
                    case SDL_PRESSED:
                    case SDL_RELEASED: {
                        const bool isPressed = sdlEvent.type == SDL_PRESSED;
                        Modifier modifier = 
                            ((sdlEvent.key.keysym.mod & KMOD_SHIFT) ? Modifier::Shift : Modifier::None) |
                            ((sdlEvent.key.keysym.mod & KMOD_CTRL) ? Modifier::Ctrl : Modifier::None) |
                            ((sdlEvent.key.keysym.mod & KMOD_ALT) ? Modifier::Alt : Modifier::None);
                        auto key = SDLToKey(sdlEvent.key.keysym.sym);
                        if(key != Key::KeyEmpty) {
                            std::lock_guard<std::mutex> lock(impl->m_mutex);
                            impl->m_queuedPresses.push_back({ key, modifier, isPressed});
                            impl->m_keyPressState[static_cast<size_t>(key)] = isPressed;
                        }
                        break;
                    } 
                    case SDL_TEXTINPUT: {
                        std::lock_guard<std::mutex> lock(impl->m_mutex);
                        impl->m_queuedCharacter.push_back({ sdlEvent.text.text[0] });
                        break;
                    }
                    
                }
            });
            return handle;
        }

        window::internal::WindowInternalEvent::Handler& GetWindowEventHandle(InternalInputKeyboardHandle& handle) {
            auto* impl = static_cast<InternalInputKeyboardImpl*>(handle.Get());
            return impl->m_windowEventHandle;
        }

    } // namespace keyboard

} // namespace hpl::input::internal