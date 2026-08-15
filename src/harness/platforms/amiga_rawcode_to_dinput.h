#ifndef _AMIGA_RAWCODES_TO_DINPUT_H_
#define _AMIGA_RAWCODES_TO_DINPUT_H_

#include <string.h>

/* DirectInput key codes */
#if 1
#include <pc-win95/dinput.h>
#else

#define DIK_ESCAPE      0x01
#define DIK_1           0x02
#define DIK_2           0x03
#define DIK_3           0x04
#define DIK_4           0x05
#define DIK_5           0x06
#define DIK_6           0x07
#define DIK_7           0x08
#define DIK_8           0x09
#define DIK_9           0x0A
#define DIK_0           0x0B
#define DIK_MINUS       0x0C
#define DIK_EQUALS      0x0D
#define DIK_BACK        0x0E
#define DIK_TAB         0x0F
#define DIK_Q           0x10
#define DIK_W           0x11
#define DIK_E           0x12
#define DIK_R           0x13
#define DIK_T           0x14
#define DIK_Y           0x15
#define DIK_U           0x16
#define DIK_I           0x17
#define DIK_O           0x18
#define DIK_P           0x19
#define DIK_LBRACKET    0x1A
#define DIK_RBRACKET    0x1B
#define DIK_RETURN      0x1C
#define DIK_A           0x1E
#define DIK_S           0x1F
#define DIK_D           0x20
#define DIK_F           0x21
#define DIK_G           0x22
#define DIK_H           0x23
#define DIK_J           0x24
#define DIK_K           0x25
#define DIK_L           0x26
#define DIK_SEMICOLON   0x27
#define DIK_APOSTROPHE  0x28
#define DIK_GRAVE       0x29
#define DIK_LSHIFT      0x2A
#define DIK_BACKSLASH   0x2B
#define DIK_Z           0x2C
#define DIK_X           0x2D
#define DIK_C           0x2E
#define DIK_V           0x2F
#define DIK_B           0x30
#define DIK_N           0x31
#define DIK_M           0x32
#define DIK_COMMA       0x33
#define DIK_PERIOD      0x34
#define DIK_SLASH       0x35
#define DIK_SPACE       0x39
#define DIK_CAPITAL     0x3A
#define DIK_F1          0x3B
#define DIK_F2          0x3C
#define DIK_F3          0x3D
#define DIK_F4          0x3E
#define DIK_F5          0x3F
#define DIK_F6          0x40
#define DIK_F7          0x41
#define DIK_F8          0x42
#define DIK_F9          0x43
#define DIK_F10         0x44
#define DIK_NUMLOCK     0x45
#define DIK_SCROLL      0x46
#define DIK_NUMPAD7     0x47
#define DIK_NUMPAD8     0x48
#define DIK_NUMPAD9     0x49
#define DIK_SUBTRACT    0x4A
#define DIK_NUMPAD4     0x4B
#define DIK_NUMPAD5     0x4C
#define DIK_NUMPAD6     0x4D
#define DIK_ADD         0x4E
#define DIK_NUMPAD1     0x4F
#define DIK_NUMPAD2     0x50
#define DIK_NUMPAD3     0x51
#define DIK_NUMPAD0     0x52
#define DIK_DECIMAL     0x53
#define DIK_DELETE      0xD3
#define DIK_LCONTROL    0x1D
#define DIK_LMENU       0x38
#define DIK_RIGHT       0xCD
#define DIK_LEFT        0xCB
#define DIK_DOWN        0xD0
#define DIK_UP          0xC8
#endif
static int amigaRawKeyToDirectInputKeyNum[256];

static void initializeAmigaRawKeyNums(void) {
    memset(amigaRawKeyToDirectInputKeyNum, 0, sizeof(amigaRawKeyToDirectInputKeyNum));
    // Przykładowe mapowania:
    // Amiga rawkeys dla liter (US QWERTY):
    // a = 0x20, b=0x35, c=0x33, d=0x22, e=0x12, ...
    amigaRawKeyToDirectInputKeyNum[0x20] = DIK_A;
    amigaRawKeyToDirectInputKeyNum[0x35] = DIK_B;
    amigaRawKeyToDirectInputKeyNum[0x33] = DIK_C;
    amigaRawKeyToDirectInputKeyNum[0x22] = DIK_D;
    amigaRawKeyToDirectInputKeyNum[0x12] = DIK_E;
    amigaRawKeyToDirectInputKeyNum[0x23] = DIK_F;
    amigaRawKeyToDirectInputKeyNum[0x24] = DIK_G;
    amigaRawKeyToDirectInputKeyNum[0x25] = DIK_H;
    amigaRawKeyToDirectInputKeyNum[0x17] = DIK_I;
    amigaRawKeyToDirectInputKeyNum[0x26] = DIK_J;
    amigaRawKeyToDirectInputKeyNum[0x27] = DIK_K;
    amigaRawKeyToDirectInputKeyNum[0x28] = DIK_L;
    amigaRawKeyToDirectInputKeyNum[0x37] = DIK_M;
    amigaRawKeyToDirectInputKeyNum[0x36] = DIK_N;
    amigaRawKeyToDirectInputKeyNum[0x18] = DIK_O;
    amigaRawKeyToDirectInputKeyNum[0x19] = DIK_P;
    amigaRawKeyToDirectInputKeyNum[0x10] = DIK_Q;
    amigaRawKeyToDirectInputKeyNum[0x13] = DIK_R;
    amigaRawKeyToDirectInputKeyNum[0x21] = DIK_S;
    amigaRawKeyToDirectInputKeyNum[0x14] = DIK_T;
    amigaRawKeyToDirectInputKeyNum[0x16] = DIK_U;
    amigaRawKeyToDirectInputKeyNum[0x34] = DIK_V;
    amigaRawKeyToDirectInputKeyNum[0x11] = DIK_W;
    amigaRawKeyToDirectInputKeyNum[0x32] = DIK_X;
    amigaRawKeyToDirectInputKeyNum[0x15] = DIK_Y;
    amigaRawKeyToDirectInputKeyNum[0x31] = DIK_Z;

    // Cyfry (górny rząd) Amiga raw keys: 1=0x01,2=0x02,...0=0x0A
    amigaRawKeyToDirectInputKeyNum[0x01] = DIK_1;
    amigaRawKeyToDirectInputKeyNum[0x02] = DIK_2;
    amigaRawKeyToDirectInputKeyNum[0x03] = DIK_3;
    amigaRawKeyToDirectInputKeyNum[0x04] = DIK_4;
    amigaRawKeyToDirectInputKeyNum[0x05] = DIK_5;
    amigaRawKeyToDirectInputKeyNum[0x06] = DIK_6;
    amigaRawKeyToDirectInputKeyNum[0x07] = DIK_7;
    amigaRawKeyToDirectInputKeyNum[0x08] = DIK_8;
    amigaRawKeyToDirectInputKeyNum[0x09] = DIK_9;
    amigaRawKeyToDirectInputKeyNum[0x0A] = DIK_0;

    // Pozostałe klawisze:
    // ; =  0x29 
    amigaRawKeyToDirectInputKeyNum[0x29] = DIK_SEMICOLON;
    // ' =  0x2A
    amigaRawKeyToDirectInputKeyNum[0x2A] = DIK_APOSTROPHE;
    // Enter = 0x44
    amigaRawKeyToDirectInputKeyNum[0x44] = DIK_RETURN;
    // Escape = 0x45
    amigaRawKeyToDirectInputKeyNum[0x45] = DIK_ESCAPE;
    // Spacja = 0x40
    amigaRawKeyToDirectInputKeyNum[0x40] = DIK_SPACE;
    // Backspace = 0x41
    amigaRawKeyToDirectInputKeyNum[0x41] = DIK_BACK;
    // Tab = 0x42
    amigaRawKeyToDirectInputKeyNum[0x42] = DIK_TAB;
    // CapsLock = 0x62
    amigaRawKeyToDirectInputKeyNum[0x62] = DIK_CAPITAL;
    // Delete = 0x46
    amigaRawKeyToDirectInputKeyNum[0x46] = DIK_DELETE;
    // Shift = 0x63
    amigaRawKeyToDirectInputKeyNum[0x63] = DIK_LSHIFT;
    // Control = 0x64
    amigaRawKeyToDirectInputKeyNum[0x64] = DIK_LCONTROL;
    // Alt = 0x65
    amigaRawKeyToDirectInputKeyNum[0x65] = DIK_LMENU;

    // F1-F10: F1=0x50,F2=0x51,...,F10=0x59
    amigaRawKeyToDirectInputKeyNum[0x50] = DIK_F1;
    amigaRawKeyToDirectInputKeyNum[0x51] = DIK_F2;
    amigaRawKeyToDirectInputKeyNum[0x52] = DIK_F3;
    amigaRawKeyToDirectInputKeyNum[0x53] = DIK_F4;
    amigaRawKeyToDirectInputKeyNum[0x54] = DIK_F5;
    amigaRawKeyToDirectInputKeyNum[0x55] = DIK_F6;
    amigaRawKeyToDirectInputKeyNum[0x56] = DIK_F7;
    amigaRawKeyToDirectInputKeyNum[0x57] = DIK_F8;
    amigaRawKeyToDirectInputKeyNum[0x58] = DIK_F9;
    amigaRawKeyToDirectInputKeyNum[0x59] = DIK_F10;

    // Arrows: Up=0x47, Down=0x48, Left=0x49, Right=0x4A
    amigaRawKeyToDirectInputKeyNum[0x4C] = DIK_UP;
    amigaRawKeyToDirectInputKeyNum[0x4D] = DIK_DOWN;
    amigaRawKeyToDirectInputKeyNum[0x4F] = DIK_LEFT;
    amigaRawKeyToDirectInputKeyNum[0x4E] = DIK_RIGHT;

    // Numpad: 0=0x60,1=0x61,2=0x62,...,9=0x69
    amigaRawKeyToDirectInputKeyNum[0x0F] = DIK_NUMPAD0;
    amigaRawKeyToDirectInputKeyNum[0x1D] = DIK_NUMPAD1;
    amigaRawKeyToDirectInputKeyNum[0x1E] = DIK_NUMPAD2;
    amigaRawKeyToDirectInputKeyNum[0x1F] = DIK_NUMPAD3;
    amigaRawKeyToDirectInputKeyNum[0x2D] = DIK_NUMPAD4;
    amigaRawKeyToDirectInputKeyNum[0x2E] = DIK_NUMPAD5;
    amigaRawKeyToDirectInputKeyNum[0x2F] = DIK_NUMPAD6;
    amigaRawKeyToDirectInputKeyNum[0x3D] = DIK_NUMPAD7;
    amigaRawKeyToDirectInputKeyNum[0x3E] = DIK_NUMPAD8;
    amigaRawKeyToDirectInputKeyNum[0x3F] = DIK_NUMPAD9;
    // Rest of the numpad keys
    amigaRawKeyToDirectInputKeyNum[0x4A] = DIK_NUMPADMINUS;
    amigaRawKeyToDirectInputKeyNum[0x43] = DIK_NUMPADENTER;
    amigaRawKeyToDirectInputKeyNum[0x5A] = DIK_LBRACKET;
    amigaRawKeyToDirectInputKeyNum[0x5B] = DIK_RBRACKET;
    amigaRawKeyToDirectInputKeyNum[0x5C] = DIK_NUMPADSLASH;
    amigaRawKeyToDirectInputKeyNum[0x5E] = DIK_NUMPADPLUS;
    amigaRawKeyToDirectInputKeyNum[0x3C] = DIK_NUMPADCOMMA;
    amigaRawKeyToDirectInputKeyNum[0x5D] = DIK_NUMPADSTAR;
    //CDTV Remote control keys
    amigaRawKeyToDirectInputKeyNum[0x6C] = DIK_PLAYPAUSE;
    amigaRawKeyToDirectInputKeyNum[0x6D] = DIK_MEDIASTOP;
    amigaRawKeyToDirectInputKeyNum[0x6E] = DIK_NEXTTRACK;
    amigaRawKeyToDirectInputKeyNum[0x6F] = DIK_PREVTRACK;
}

// Tablica stanów klawiszy używana przez kod DirectInput
static uint8_t directinput_key_state[256];

#endif /* _AMIGA_RAWCODES_TO_DINPUT_H_ */
