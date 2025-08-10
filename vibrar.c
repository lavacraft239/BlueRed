#include <stdio.h>
#include <stdlib.h>

int main() {
    system("termux-notification --title 'Juan' --content 'Sistema con virus'");

    // Vibrar el teléfono dos veces
    system("termux-vibrate -d 200");
    system("termux-vibrate -d 200");
    
    return 0;
}
