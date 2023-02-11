#include <input/InputKeyboardDevice.h>


namespace hpl::input {
    char KeyToCharacter(Key key, Modifier modifier) {
        switch(key) {
            case Key::KeyA:
                break;
            case Key::KeyB:
                break;
            case Key::KeyC:
                break;
            case Key::KeyD:
                break;
            case Key::KeyE:
                break;
            case Key::KeyF:
                break;
            case Key::KeyG:
                break;
            case Key::KeyH:
                break;
            case Key::KeyI:
                break;
            case Key::KeyJ:
                break;
            case Key::KeyK:
                break;
            case Key::KeyL:
                break;
            case Key::KeyM:
                break;
            case Key::KeyN:
                break;
            case Key::KeyO:
                break;
            case Key::KeyP:
                break;
            case Key::KeyQ:
                break;
            case Key::KeyR:
                break;
            case Key::KeyS:
                break;
            case Key::KeyT:
                break;
            case Key::KeyU:
                break;
            case Key::KeyV:
                break;
            case Key::KeyW:
                break;
            case Key::KeyX:
                break;
            case Key::KeyY:
                break;
            case Key::KeyZ:
                break;
            default:
                break;
        }
        return 0;
    }


	Modifier operator|(Modifier lhs, Modifier rhs) {
        return static_cast<Modifier>(static_cast<int>(lhs) | static_cast<int>(rhs));
    }
    bool operator&(Modifier lhs, Modifier rhs) {
        return static_cast<int>(lhs) & static_cast<int>(rhs);
    }
}