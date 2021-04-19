#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

int udpSocket;
struct sockaddr_in serverAddr;
struct sockaddr_in cameraAddr;

void closedown() {
    sendto(udpSocket, "JHCMD\xd0\x02", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    IMG_Quit();
    SDL_Quit();
}

int main(int argc, char **argv) {

    uint8_t frame[131072];
    uint8_t buffer[1450];
    int framepos;
    int frameno;
    int packetno;

    SDL_Window *window;
    SDL_Surface *windowSurface;
    SDL_Event event;

    udpSocket = socket(PF_INET, SOCK_DGRAM, 0);

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(10900);
    serverAddr.sin_addr.s_addr = inet_addr("0.0.0.0");
    memset(serverAddr.sin_zero, '\0', sizeof(serverAddr.sin_zero));

    cameraAddr.sin_family = AF_INET;
    cameraAddr.sin_port = htons(20000);
    cameraAddr.sin_addr.s_addr = inet_addr("192.168.29.1");
    memset(cameraAddr.sin_zero, '\0', sizeof(cameraAddr.sin_zero));

    if (bind(udpSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
        fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
        return -1;
    }


    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_JPG);
    window = SDL_CreateWindow("WiFi Microscope",
        SDL_WINDOWPOS_UNDEFINED,
        SDL_WINDOWPOS_UNDEFINED,
        1280,
        720,
        SDL_WINDOW_SHOWN
    );

    windowSurface = SDL_GetWindowSurface(window);

    atexit(closedown);

    sendto(udpSocket, "JHCMD\x10\x00", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    sendto(udpSocket, "JHCMD\x20\x00", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));

    while (1) {

        if (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    closedown();
                    exit(0);
            }
        }

        int nBytes = recvfrom(udpSocket, buffer, sizeof(buffer), 0, NULL, NULL);

        if (nBytes > 8) {
            frameno = buffer[0] | (buffer[1] << 8);
            packetno = buffer[3];

            if (packetno == 0) {
                if (framepos > 0) {
                    // New frame starting, display the old one.
                    SDL_RWops *jpg = SDL_RWFromMem(frame, framepos);
                    SDL_Surface *image = IMG_LoadJPG_RW(jpg);
                    SDL_BlitSurface(image, NULL, windowSurface, NULL);
                    SDL_FreeSurface(image);
                    SDL_RWclose(jpg);
                    SDL_UpdateWindowSurface(window);
                }
                framepos = 0;
                printf("Getting frame %d\n", frameno);

                if (frameno % 50 == 0) {
                    sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
                }
            }

            memcpy(frame + framepos, buffer + 8, nBytes - 8);
            framepos += (nBytes - 8);
        }
    }
}
