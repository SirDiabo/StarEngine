#include "KeyLayout.h"
#include "raylib.h"
#include <cctype>

namespace {

int cachedKeys[26];
double cachedAt = -10.0;

void Refresh() {
    double now = GetTime();
    if (now - cachedAt < 1.0) return;
    cachedAt = now;
    for (int i = 0; i < 26; i++) cachedKeys[i] = KEY_A + i;
    for (int i = 0; i < 26; i++) {
        int physical = KEY_A + i;
        const char* name = GetKeyName(physical);
        if (!name || !name[0] || name[1] != '\0') continue;
        int c = tolower((unsigned char)name[0]);
        if (c >= 'a' && c <= 'z') cachedKeys[c - 'a'] = physical;
    }
}

}

namespace KeyLayout {

int KeyForLetter(char letter) {
    int c = tolower((unsigned char)letter);
    if (c < 'a' || c > 'z') return 0;
    Refresh();
    return cachedKeys[c - 'a'];
}

bool Pressed(char letter) {
    int key = KeyForLetter(letter);
    return key != 0 && IsKeyPressed(key);
}

bool PressedOrRepeat(char letter) {
    int key = KeyForLetter(letter);
    return key != 0 && (IsKeyPressed(key) || IsKeyPressedRepeat(key));
}

bool Down(char letter) {
    int key = KeyForLetter(letter);
    return key != 0 && IsKeyDown(key);
}

}
