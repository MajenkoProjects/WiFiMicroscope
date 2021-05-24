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
#include <linux/videodev2.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include "testcard.h"

int udpSocket;
struct sockaddr_in serverAddr;
struct sockaddr_in cameraAddr;

pthread_t cameraThread;

uint8_t imageFrame[1310720];

int imageFrameSize = 0;
int imageReady = 0;
int running = 1;
int online = 0;

int brightness = 0;
float contrast = 1.0;

SDL_Window *window;
SDL_Surface *windowSurface;
SDL_Event event;

char *video = NULL;

uint32_t ip;

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
    cameraAddr.sin_addr.s_addr = ip;
    memset(cameraAddr.sin_zero, '\0', sizeof(cameraAddr.sin_zero));

    if (bind(udpSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0) {
        fprintf(stderr, "Error binding socket: %s\n", strerror(errno));
        exit(10);
    }

    sendto(udpSocket, "JHCMD\x10\x00", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    sendto(udpSocket, "JHCMD\x20\x00", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
    sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));

    int olcount = 0;
        int lastPacket = -1;

    while (running) {

        fd_set rfd;
        fd_set efd;
        fd_set wfd;

        struct timeval to;

        FD_ZERO(&rfd);
        FD_ZERO(&efd);
        FD_ZERO(&wfd);

        FD_SET(udpSocket, &rfd);
        //FD_SET(udpSocket, &efd);
        //FD_SET(udpSocket, &wfd);

        to.tv_sec = 1;
        to.tv_usec = 0;

        int t = select(udpSocket + 1, &rfd, &wfd, &efd, &to);


        if (t > 0) {
            int nBytes = recvfrom(udpSocket, buffer, sizeof(buffer), 0, NULL, NULL);

            if (nBytes > 8) {
                online = 1;
                olcount = 0;
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
                } else if (packetno != (lastPacket + 1)) {
                    printf("Dropped packet detected!\n");
                }
                lastPacket = packetno;

                memcpy(receivingFrame + framepos, buffer + 8, nBytes - 8);
                framepos += (nBytes - 8);
            }
        } else {
            online = 0;
            olcount++;
            if (olcount > 10) {
                printf("Pinging device...\n");
                olcount = 0;
                sendto(udpSocket, "JHCMD\x10\x00", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
                sendto(udpSocket, "JHCMD\x20\x00", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
                sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
                sendto(udpSocket, "JHCMD\xd0\x01", 7, 0, (struct sockaddr *)&cameraAddr, sizeof(cameraAddr));
            }
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
                                if (brightness > 255) brightness = 255;
                                break;
                            case '[':
                                brightness--;
                                if (brightness < -255) brightness = -255;
                                break;
                            case '#':
                                contrast += 0.1;
                                if (contrast > 5.0) contrast = 5.0;
                                break;
                            case '\'':
                                contrast -= 0.1;
                                if (contrast < 0.1) contrast = 0.1;
                                break;
                            case 'r':
                                contrast = 1.0;
                                brightness = 0;
                                break;
                            case 's': {
                                char filename[50];
                                snprintf(filename, 50, "microscope_%lu.jpg", time(NULL));
                                IMG_SaveJPG(windowSurface, filename, 75);
                                printf("Saved snapshot to %s\n", filename);
                            }
                        }
                    }
                    break;
            }
        }

        if (online) {
            if (imageReady == 1) {

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
                imageReady = 0;
            } else {
                SDL_Delay(30);
            }
        } else {
            SDL_RWops *jpg = SDL_RWFromMem(testcard, testcard_len);
            SDL_Surface *image = IMG_LoadJPG_RW(jpg);
            SDL_RWclose(jpg);
            SDL_BlitSurface(image, NULL, windowSurface, NULL);
            SDL_FreeSurface(image);
            SDL_UpdateWindowSurface(window);
            SDL_Delay(30);
        }
    }

    IMG_Quit();
    SDL_Quit();
    return 0;
}


int main(int argc, char **argv) {
    int opt;

    ip = inet_addr("192.168.29.1");

    while ((opt = getopt(argc, argv, "d:hvi:")) != -1) {
        switch (opt) {
            case 'v':
                printf("WiFi Microscope Receiver V1.0\n(c) 2021 Majenko Technologies\n");
                break;
            case 'd':
                video = optarg;
                break;
            case 'i':
                ip = inet_addr(optarg);
                break;
            case 'h':
                printf("Usage: microscope [-hv] [-d /dev/videoX] [-i ip]\n");
                printf("     h: Show this help message\n");
                printf("     v: Show the version and copyright information\n");
                printf("     d: Send the video to the specified v4l2loopback device\n");
                printf("     i: Connect to different IP address instead of the standard 192.168.29.1\n");
                return 0;
                break;
        }
    }




    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_create(&cameraThread, &attr, &receiveFromCamera, NULL);


    if (video != NULL) {
        if (access(video, R_OK | W_OK) != 0) {
            fprintf(stderr, "Error: Unable to access %s\n", video);
            return 10;
        }
        printf("Sending video to %s\n", video);

        int fd = open(video, O_RDWR);
        if (!fd) {
            fprintf(stderr, "Error: Unable to access %s: %s\n", video, strerror(errno));
            return 10;
        }

        struct v4l2_capability vid_caps;
        struct v4l2_format vid_format;

        IMG_Init(IMG_INIT_JPG);

        int rv = ioctl(fd, VIDIOC_QUERYCAP, &vid_caps);
        if (rv < 0) {
            fprintf(stderr, "Error: Unable to probe video device: %s\n", strerror(errno));
            return 10;
        }

        memset(&vid_format, 0, sizeof(vid_format));

        vid_format.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
        vid_format.fmt.pix.width = 1280;
        vid_format.fmt.pix.height = 720;
        vid_format.fmt.pix.sizeimage = 1280 * 720 * 3;
        vid_format.fmt.pix.pixelformat = v4l2_fourcc('R','G','B','3');
        vid_format.fmt.pix.field = V4L2_FIELD_NONE;
        vid_format.fmt.pix.bytesperline = 1280 * 3;
        vid_format.fmt.pix.colorspace = V4L2_COLORSPACE_SRGB;

        rv = ioctl(fd, VIDIOC_S_FMT, &vid_format);
        if (rv < 0) {
            fprintf(stderr, "Error: Unable to set correct video mode: %s\n", strerror(errno));
            return 10;
        }

        
        while (running == 1) {
            if (online) {
                if (imageReady == 1) {
                    SDL_RWops *jpg = SDL_RWFromMem(imageFrame, imageFrameSize);
                    SDL_Surface *image = IMG_LoadJPG_RW(jpg);
                    SDL_RWclose(jpg);
                    SDL_LockSurface(image);
                    uint8_t *pixels = (uint8_t *)image->pixels;

                    if (write(fd, pixels, vid_format.fmt.pix.sizeimage) < 0) {
                        fprintf(stderr, "Error: Unable to write %d bytes to video device: %s\n", vid_format.fmt.pix.sizeimage, strerror(errno));
                        close(fd);
                        return 10;
                    }
                    SDL_UnlockSurface(image);
                    SDL_FreeSurface(image);
                    imageReady = 0;
                } else {
                    SDL_Delay(3);
                }
            } else {
                SDL_RWops *jpg = SDL_RWFromMem(testcard, testcard_len);
                SDL_Surface *image = IMG_LoadJPG_RW(jpg);
                SDL_RWclose(jpg);
                SDL_LockSurface(image);
                uint8_t *pixels = (uint8_t *)image->pixels;

                if (write(fd, pixels, vid_format.fmt.pix.sizeimage) < 0) {
                    fprintf(stderr, "Error: Unable to write %d bytes to video device: %s\n", vid_format.fmt.pix.sizeimage, strerror(errno));
                    close(fd);
                    return 10;
                }
                SDL_UnlockSurface(image);
                SDL_FreeSurface(image);
                sleep(1);
            }
        }
        close(fd);
            

    } else {
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

        displayImage(NULL);

    }


    pthread_join(cameraThread, NULL);
    return 0;

}
