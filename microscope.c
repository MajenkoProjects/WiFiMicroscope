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
#include <pthread.h>

int udpSocket;
struct sockaddr_in serverAddr;
struct sockaddr_in cameraAddr;

pthread_t cameraThread;

uint8_t imageFrame[1310720];

int imageFrameSize = 0;
int imageReady = 0;
int running = 1;

int brightness = 0;
float contrast = 1.0;

SDL_Window *window;
SDL_Surface *windowSurface;
SDL_Event event;

void *receiveFromCamera(void *none) {
    uint8_t receivingFrame[1310720];
    uint8_t buffer[1450];
    int framepos = 0;
    int frameno = 0;
    int packetno = 0;

    udpSocket = socket(PF_INET, SOCK_DGRAM, 0);

    if (!udpSocket) {
        fprintf(stderr, "Unable to create socket: %s\n", strerror(errno));
        exit(10);
    }
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
        exit(10);
    }

    sendto(udpSocket, "JHCMD\x10\x00", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    sendto(udpSocket, "JHCMD\x20\x00", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));

    while (running) {
        int nBytes = recvfrom(udpSocket, buffer, sizeof(buffer), 0, NULL, NULL);

        if (nBytes > 8) {
            frameno = buffer[0] | (buffer[1] << 8);
            packetno = buffer[3];

            if (packetno == 0) {
                if (framepos > 0) {
                    memcpy(imageFrame, receivingFrame, framepos);
                    imageFrameSize = framepos;
                    imageReady = 1;
                }
                framepos = 0;

                if (frameno % 50 == 0) {
                    sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
                }
            }

            memcpy(receivingFrame + framepos, buffer + 8, nBytes - 8);
            framepos += (nBytes - 8);
        }
    }
    sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    return 0;
}


void *displayImage(void *none) {
    int invert = 0;
    int mono = 0;
    int i;

    while (running) {
        if (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym) {
                        switch (event.key.keysym.sym) {
                            case 'q':
                                running = 0;
                                break;
                            case 'm':
                                mono = 1 - mono;
                                break;
                            case 'i':
                                invert = 1 - invert;
                                break;
                            case ']':
                                brightness++;
                                if (brightness > 127) brightness = 127;
                                break;
                            case '[':
                                brightness--;
                                if (brightness < -127) brightness = -127;
                                break;
                            case '#':
                                contrast += 0.1;
                                if (contrast > 5.0) contrast = 5.0;
                                break;
                            case '\'':
                                contrast -= 0.1;
                                if (contrast < 0.1) contrast = 0.1;
                                break;
                        }
                    }
                    break;
            }
        }

        if (imageReady == 1) {
            imageReady = 0;
//            SDL_Delay(10);

            SDL_RWops *jpg = SDL_RWFromMem(imageFrame, imageFrameSize);
            SDL_Surface *image = IMG_LoadJPG_RW(jpg);
            SDL_RWclose(jpg);
            if (image) {
                uint8_t *pixels = (uint8_t *)(image->pixels);

                SDL_LockSurface(image);

                for (i = 0; i < (1280 * 720 * 3); i += 3) {
                    int r = pixels[i];
                    int g = pixels[i + 1];
                    int b = pixels[i + 2];

                    if (mono) {
                        float lum = 0.299 * r + 0.587 * g + 0.114 * b;
                        int l = lum;
                        if (l > 255) l = 255;
                        r = g = b = lum;
                    }

                    if (invert) {
                        r = 255 - r;
                        g = 255 - g;
                        b = 255 - b;
                    }

                    r += brightness;
                    g += brightness;
                    b += brightness;

                    r *= contrast;
                    g *= contrast;
                    b *= contrast;
            
                    if (r > 255) r = 255;
                    if (g > 255) g = 255;
                    if (b > 255) b = 255;

                    if (r < 0) r = 0;
                    if (g < 0) g = 0;
                    if (b < 0) b = 0;

                    pixels[i] = r;
                    pixels[i + 1] = g;
                    pixels[i + 2] = b;
                }

                SDL_UnlockSurface(image);

                SDL_BlitSurface(image, NULL, windowSurface, NULL);
                SDL_FreeSurface(image);
                SDL_UpdateWindowSurface(window);
            }
        } else {
            SDL_Delay(10);
        }
    }

    IMG_Quit();
    SDL_Quit();
    return 0;
}


int main(int argc, char **argv) {
    pthread_attr_t attr;
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
    
    atexit(SDL_Quit);
    pthread_attr_init(&attr);
    pthread_create(&cameraThread, &attr, &receiveFromCamera, NULL);

    displayImage(NULL);

    pthread_join(cameraThread, NULL);

    return 0;

}
