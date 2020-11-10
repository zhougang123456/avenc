#include "avencoder.hpp"

#include<windows.h>

int main(int argc, char** argv)
{
    char* codec_name = (char*)"libx264";
    int width = 720; int height = 360;
    AvEncoder* encoder = new AvEncoder();
    if (!encoder->init(codec_name, width, height)) {
        return 0;
    }
    unsigned char* buffer = new unsigned char[width * height * 4];
    int i = 0;
 
    while (1){
        
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                buffer[x * 4 + y * width * 4] = i * 10;
                buffer[x * 4 + 1 + y * width * 4] = i * 10;
                buffer[x * 4 + 2 + y * width * 4] = 255 - i*10;
            }
        }
        i++;
        encoder->encode(buffer);
        Sleep(30);
        if (i > 30) {
            break;
        }
    }
    while (1) {
        printf("wait");
        Sleep(1000);
    }
    return 0;
}