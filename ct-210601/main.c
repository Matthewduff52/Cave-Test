//Author: Matthew Duff
//Version: CT04202026a

#include <stdio.h>
#include <conio.h>
#include <i86.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <dos.h>

#define SCREENW 320
#define SCREENH 200

unsigned char worldMap[24][24][24] = {0};
unsigned char shadowMap[24][24];

double posX = 12.0, posY = 12.0, posZ = 13.62;
double dirX = -1.0, dirY = 0.0, dirZ = 0.0;
double planeX = 0.0, planeY = 0.66, planeZ = 0.50;
unsigned char key_map[128] = {0};
void (__interrupt __far *old_int9)(void);
int pitch = 0;

void get_mouse_mickeys(int *dx, int *dy) {
    union REGS regs;
    regs.w.ax = 0x000B;
    int386(0x33, &regs, &regs);
    *dx = (short)regs.w.cx;
    *dy = (short)regs.w.dx;
}

void center_mouse() {
    union REGS regs;
    regs.w.ax = 0x0004;
    regs.w.cx = 320;
    regs.w.dx = 100;
    int386(0x33, &regs, &regs);
}

void __interrupt __far new_int9(void) {
    unsigned char scancode = inp(0x60);
    if (scancode < 128) key_map[scancode] = 1;
    else key_map[scancode - 128] = 0;
    outp(0x20, 0x20);
}

void print_string(char *str, int row, int col) {
    union REGS regs;
    //struct SREGS sregs;
    
    /* Set cursor position: DH = row, DL = column */
    regs.h.ah = 0x02;
    regs.h.bh = 0;
    regs.h.dh = row;
    regs.h.dl = col;
    int386(0x10, &regs, &regs);

    /* Print characters one by one */
    while (*str) {
        regs.h.ah = 0x0E;
        regs.h.al = *str;
        regs.h.bl = 0x0F; // White color
        int386(0x10, &regs, &regs);
        str++;
    }
}

int main(void) {
    union REGS r;
    char *video = (char *)0xA0000;
    char *buffer = (char *)malloc(SCREENW * SCREENH);
    /* ALL DECLARATIONS AT TOP */
    int x, y, z, mX, mY, mZ, stX, stY, stZ, hit, side;
    float oldDirX, oldPlaneX, angle;
    int mdx, mdy;
    float mouseSensitivity = 0.005f;
    float pitchSensitivity = 2.0f;
    double moveSpeed = 0.05;
    double camX, camY, rDirX, rDirY, rDirZ;
    double dDX, dDY, dDZ, sDX, sDY, sDZ;
    unsigned char color;
    int step = 3; // Set to 2 for 2x2 blocks, or 4 for 4x4 blocks
    int ix, iy;
    double nextX, nextY;
    double vertVelocity = 0.0;
    double gravity = 0.012;  /* Adjust for how heavy you feel */
    double jumpPower = 0.15; /* Adjust for how high you jump */
    float maxDist = 5.0f; // Max reach for placing/breaking
    float dist = 0;
    int leftClick;
    int rightClick;
    // Static flag to prevent "machine gun" placement (one action per click)
    static int mousePressed = 0;
    double rayDirX, rayDirY, rayDirZ;
    int stepX, stepY, stepZ;
    int curX, curY, curZ;
    int prevX, prevY, prevZ;
    int inventorySelect = 1;
    char invMsg[20];

    if (buffer == NULL) return 1;

    /* Initialize Map */
    for(x=0; x<24; x++) {
        for(y=0; y<24; y++) {
            worldMap[x][y][0] = 2;
            worldMap[x][y][1] = 2;
            worldMap[x][y][2] = 2;
            worldMap[x][y][3] = 2;
            worldMap[x][y][4] = 2;
            worldMap[x][y][5] = 2;
            worldMap[x][y][6] = 2;
            worldMap[x][y][7] = 2;
            worldMap[x][y][8] = 2;
            worldMap[x][y][9] = 2;
            worldMap[x][y][10] = 2;
            worldMap[x][y][11] = 1;

            shadowMap[x][y] = 0;
            for(z=23; z>=0; z--) {
                if(worldMap[x][y][z] > 0) {
                    shadowMap[x][y] = z;
                    break;
                }
            }
        }
    }

    old_int9 = _dos_getvect(0x09);
    _dos_setvect(0x09, new_int9);

    r.w.ax = 0x0013; int386(0x10, &r, &r);
    r.w.ax = 0; int386(0x33, &r, &r);

    while (!key_map[1]) {
        memset(buffer, 0x64, SCREENW * SCREENH);
        get_mouse_mickeys(&mdx, &mdy);

        for (y = 0; y < SCREENH; y += step) {
            for (x = 0; x < SCREENW; x += step) {
                camX = 2.0 * x / SCREENW - 1.0;
                camY = 1.0 - 2.0 * y / SCREENH;
                rDirX = dirX + planeX * camX;
                rDirY = dirY + planeY * camX;
                rDirZ = dirZ + planeZ * camY - (pitch / 100.0);

                mX = (int)posX; mY = (int)posY; mZ = (int)posZ;
                dDX = fabs(1.0 / rDirX); dDY = fabs(1.0 / rDirY); dDZ = fabs(1.0 / rDirZ);

                if (rDirX < 0) { stX = -1; sDX = (posX - mX) * dDX; }
                else { stX = 1; sDX = (mX + 1.0 - posX) * dDX; }
                if (rDirY < 0) { stY = -1; sDY = (posY - mY) * dDY; }
                else { stY = 1; sDY = (mY + 1.0 - posY) * dDY; }
                if (rDirZ < 0) { stZ = -1; sDZ = (posZ - mZ) * dDZ; }
                else { stZ = 1; sDZ = (mZ + 1.0 - posZ) * dDZ; }

                hit = 0;
                while (!hit) {
                    if (sDX < sDY && sDX < sDZ) { 
                        sDX += dDX; mX += stX; 
                        side = (stX > 0) ? 0 : 1; // 0: West, 1: East
                    } 
                    else if (sDY < sDZ) { 
                        sDY += dDY; mY += stY; 
                        side = (stY > 0) ? 2 : 3; // 2: North, 3: South
                    } 
                    else { 
                        sDZ += dDZ; mZ += stZ; 
                        side = (stZ > 0) ? 4 : 5; // 4: Bottom, 5: Top
                    }

                    if (mX<0 || mX>=24 || mY<0 || mY>=24 || mZ<0 || mZ>=24) break;
                    if (worldMap[mX][mY][mZ] > 0) hit = 1;
                }

                if (hit) {
                    if (worldMap[mX][mY][mZ] == 1) { //Grass block
                        if (side == 0) {//west
                            if (mZ < shadowMap[mX-1][mY]) {color = 0x72 + 72; // Shadow
                            } else {color = 0x72;}}
                        if (side == 1) {//east
                            if (mZ < shadowMap[mX+1][mY]) {color = 0x72 + 72; // Shadow
                            } else {color = 0x72;}}
                        if (side == 2) {//north
                            if (mZ < shadowMap[mX][mY-1]) {color = 0x72; // Shadow
                            } else {color = 0x06;}}
                        if (side == 3) {//south
                            if (mZ < shadowMap[mX][mY+1]) {color = 0x72; // Shadow
                            } else {color = 0x06;}}
                        if (side == 4) color = 0x72 + 72; // bottom
                        if (side == 5) {//top
                            if (mZ < shadowMap[mX][mY]) {color = 0x2F + 72; // Shadow
                            } else {color = 0x2F;}}

                        //check worldMap[x][y]. for side 5 if not open sky above, color+= 72
                        // for sides 0-4, check neighboring worldMap[x][y]
                    }

                    if (worldMap[mX][mY][mZ] == 2) { // Stone block
                        if (side == 0) {//west
                            if (mZ < shadowMap[mX-1][mY]) {color = 0x1A - 8; // Shadow
                            } else {color = 0x1A;}}
                        if (side == 1) {//east
                            if (mZ < shadowMap[mX+1][mY]) {color = 0x1A - 8; // Shadow
                            } else {color = 0x1A;}}
                        if (side == 2) {//north
                            if (mZ < shadowMap[mX][mY-1]) {color = 0x1C - 8; // Shadow
                            } else {color = 0x1C;}}
                        if (side == 3) {//south
                            if (mZ < shadowMap[mX][mY+1]) {color = 0x1C - 8; // Shadow
                            } else {color = 0x1C;}}
                        if (side == 4) color = 0x1E - 8; // bottom
                        if (side == 5) {//top
                            if (mZ < shadowMap[mX][mY]) {color = 0x1E - 8; // Shadow
                            } else {color = 0x1E;}}
                    }

                    if (worldMap[mX][mY][mZ] == 3) { // Wood block
                        if (side == 0) {//top
                            if (mZ < shadowMap[mX-1][mY]) {color = 0x43 + 72; // Shadow
                            } else {color = 0x43;}}
                        if (side == 1) {//top
                            if (mZ < shadowMap[mX+1][mY]) {color = 0x43 + 72; // Shadow
                            } else {color = 0x43;}}
                        if (side == 2) {//top
                            if (mZ < shadowMap[mX][mY-1]) {color = 0x44 + 72; // Shadow
                            } else {color = 0x44;}}
                        if (side == 3) {//top
                            if (mZ < shadowMap[mX][mY+1]) {color = 0x44 + 72; // Shadow
                            } else {color = 0x44;}}
                        if (side == 4) color = 0x5C + 72; // bottom
                        if (side == 5) {//top
                            if (mZ < shadowMap[mX][mY]) {color = 0x5C + 72; // Shadow
                            } else {color = 0x5C;}}
                    }

                    /* FILL THE BLOCK: Draw a step x step square of this color */
                    for (iy = 0; iy < step; iy++) {
                        for (ix = 0; ix < step; ix++) {
                            if ((y + iy) < SCREENH && (x + ix) < SCREENW) {
                                buffer[(y + iy) * SCREENW + (x + ix)] = color;
                            }
                        }
                    }
                }
            }
        }

        /* Simple crosshair at center (160, 100) */
        buffer[100 * SCREENW + 160] = 0x0F; // White dot
        buffer[100 * SCREENW + 161] = 0x0F;
        buffer[100 * SCREENW + 159] = 0x0F;
        buffer[101 * SCREENW + 160] = 0x0F;
        buffer[99 * SCREENW + 160] = 0x0F;

        memcpy(video, buffer, SCREENW * SCREENH);

        /* Input */
        if (key_map[0x11]) {
            nextX = posX + dirX * moveSpeed;
            nextY = posY + dirY * moveSpeed;
            /* Check X, Y, and current Z layers */
            if (worldMap[(int)nextX][(int)posY][(int)posZ] == 0 && worldMap[(int)nextX][(int)posY][(int)(posZ-1)] == 0) {posX = nextX;}
            if (worldMap[(int)posX][(int)nextY][(int)posZ] == 0 && worldMap[(int)posX][(int)nextY][(int)(posZ-1)] == 0) {posY = nextY;}
        }
            
        if (key_map[0x1F]) {
            nextX = posX - dirX * moveSpeed;
            nextY = posY - dirY * moveSpeed;
            /* Check X, Y, and current Z layers */
            if (worldMap[(int)nextX][(int)posY][(int)posZ] == 0 && worldMap[(int)nextX][(int)posY][(int)(posZ-1)] == 0) {posX = nextX;}
            if (worldMap[(int)posX][(int)nextY][(int)posZ] == 0 && worldMap[(int)posX][(int)nextY][(int)(posZ-1)] == 0) {posY = nextY;}
        }
        
        if (key_map[0x1E]) {
            nextX = posX - dirY * moveSpeed;
            nextY = posY + dirX * moveSpeed;
            /* Check X, Y, and current Z layers */
            if (worldMap[(int)nextX][(int)posY][(int)posZ] == 0 && worldMap[(int)nextX][(int)posY][(int)(posZ-1)] == 0) {posX = nextX;}
            if (worldMap[(int)posX][(int)nextY][(int)posZ] == 0 && worldMap[(int)posX][(int)nextY][(int)(posZ-1)] == 0) {posY = nextY;}
        }
        
        if (key_map[0x20]) {
            nextX = posX + dirY * moveSpeed;
            nextY = posY - dirX * moveSpeed;
            /* Check X, Y, and current Z layers */
            if (worldMap[(int)nextX][(int)posY][(int)posZ] == 0 && worldMap[(int)nextX][(int)posY][(int)(posZ-1)] == 0) {posX = nextX;}
            if (worldMap[(int)posX][(int)nextY][(int)posZ] == 0 && worldMap[(int)posX][(int)nextY][(int)(posZ-1)] == 0) {posY = nextY;}
        }

        //select keys for inventory blocks
        if (key_map[0x02]) {inventorySelect = 1; strcpy(invMsg, "Grass");} // Row 1, Column 1
        if (key_map[0x03]) {inventorySelect = 2; strcpy(invMsg, "Stone");} // Row 1, Column 1
        if (key_map[0x04]) {inventorySelect = 3; strcpy(invMsg, "Wood");} // Row 1, Column 1
        print_string(invMsg, 1, 1);

        if (key_map[0x39] && vertVelocity == 0) {
            vertVelocity = jumpPower;
        }
        posZ += vertVelocity;
        vertVelocity -= gravity;
        
        if (worldMap[(int)posX][(int)posY][(int)(posZ - 1.62)] != 0) {
         vertVelocity = 0;
            posZ = (int)(posZ - 1.62) + 1.0 + 1.62;
        }
        if (worldMap[(int)posX][(int)posY][(int)(posZ + 0.1)] != 0) {
            if (vertVelocity > 0) vertVelocity = 0; /* Stop upward momentum */
        }

        // Check out of bounds
        if (posX > 24) {posX = 24;}
        if (posY > 24) {posY = 24;}
        if (posZ > 24) {posZ = 24;}
        if (posX < 0) {posX = 0;}
        if (posY < 0) {posY = 0;}
        if (posZ < 1.62) {posZ = 1.62;}

        if (mdx != 0) {
            angle = -mdx * mouseSensitivity;
            oldDirX = (float)dirX;
            dirX = dirX * cos(angle) - dirY * sin(angle);
            dirY = oldDirX * sin(angle) + dirY * cos(angle);
            oldPlaneX = (float)planeX;
            planeX = planeX * cos(angle) - planeY * sin(angle);
            planeY = oldPlaneX * sin(angle) + planeY * cos(angle);
        }
        pitch += (int)(mdy * pitchSensitivity);
        if (pitch > 180) pitch = 180;
        if (pitch < -180) pitch = -180;
        center_mouse();

        /* Mouse Click Interaction */
        r.w.ax = 0x0003; 
        int386(0x33, &r, &r);
        leftClick = r.w.bx & 1;
        rightClick = r.w.bx & 2;

        if ((leftClick || rightClick) && !mousePressed) {
            mousePressed = 1;

            // Use the center-screen ray direction (camX=0, camY=0)
            rayDirX = dirX;
            rayDirY = dirY;
            rayDirZ = dirZ - (pitch / 100.0);

            curX = (int)posX, curY = (int)posY, curZ = (int)posZ;
            prevX = curX, prevY = curY, prevZ = curZ;

            dDX = fabs(1.0 / rayDirX);
            dDY = fabs(1.0 / rayDirY);
            dDZ = fabs(1.0 / rayDirZ);

            if (rayDirX < 0) { stepX = -1; sDX = (posX - curX) * dDX; }
            else { stepX = 1; sDX = (curX + 1.0 - posX) * dDX; }
            if (rayDirY < 0) { stepY = -1; sDY = (posY - curY) * dDY; }
            else { stepY = 1; sDY = (curY + 1.0 - posY) * dDY; }
            if (rayDirZ < 0) { stepZ = -1; sDZ = (posZ - curZ) * dDZ; }
            else { stepZ = 1; sDZ = (curZ + 1.0 - posZ) * dDZ; }

            hit = 0;

            while (!hit && dist < maxDist) {
                prevX = curX; prevY = curY; prevZ = curZ;
        
                if (sDX < sDY && sDX < sDZ) { 
                    dist = sDX; sDX += dDX; curX += stepX; 
                } else if (sDY < sDZ) { 
                    dist = sDY; sDY += dDY; curY += stepY; 
                } else { 
                    dist = sDZ; sDZ += dDZ; curZ += stepZ; 
                }

                if (curX < 0 || curX >= 24 || curY < 0 || curY >= 24 || curZ < 0 || curZ >= 24) break;
                if (worldMap[curX][curY][curZ] > 0) hit = 1;
            }

            if (hit) {
                if (leftClick) {
                    worldMap[curX][curY][curZ] = 0; // Remove block
                    // Update shadow map for this column
                    shadowMap[curX][curY] = 0;
                    for(z=23; z>=0; z--) {
                        if(worldMap[curX][curY][z] > 0) {
                            shadowMap[curX][curY] = z;
                            break;
                        }
                    }
                } else if (rightClick) {
                    // Ensure we don't place a block inside our own head
                    if (!(prevX == (int)posX && prevY == (int)posY && (prevZ == (int)posZ || prevZ == (int)(posZ-1)))) {
                        if (inventorySelect == 1){worldMap[prevX][prevY][prevZ] = 1;} // Place grass block
                        if (inventorySelect == 2){worldMap[prevX][prevY][prevZ] = 2;} // Place stone block
                        if (inventorySelect == 3){worldMap[prevX][prevY][prevZ] = 3;} // Place wood block
                    }
                    // If the new block is higher than the current height map value, update it
                    if (prevZ > shadowMap[prevX][prevY]) {
                        shadowMap[prevX][prevY] = prevZ;
                    }
                }
            }
        } else if (!leftClick && !rightClick) {
            mousePressed = 0; // Reset flag when button is released
        }
    }

    free(buffer);
    r.w.ax = 0x0003; int386(0x10, &r, &r);
    _dos_setvect(0x09, old_int9);
    return 0;
}


