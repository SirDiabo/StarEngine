#pragma once

namespace KeyLayout {

int KeyForLetter(char letter);
bool Pressed(char letter);
bool PressedOrRepeat(char letter);
bool Down(char letter);

}
