#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "SDL.h"

#define register unsigned char
#define hexfont 5
#define SCALEFACTOR 10
//#define RAND_MAX = 256

void execute_opcode(unsigned short);
void initialize();
const unsigned char* keyPressed();
void cycle();
void loadgame(const char* filename);
void clear_display();
void load_fonts();
void reset();
void updateDisplay();
void printMem();
void updateTimers();
void getKeys();

//Screen dimension constants
const int SCREEN_WIDTH = 64 * SCALEFACTOR;
const int SCREEN_HEIGHT = 32 * SCALEFACTOR;
const char PPR = 64;
unsigned char pixel;
unsigned char drawflag = 0;
char booty = 99;
SDL_Rect rectangle = {0, 0, SCALEFACTOR, SCALEFACTOR};
SDL_Surface* screenSurface;
SDL_Window* window;
const SDL_Rect* rectpointer;

unsigned char memory[4096];
//interpeter takes the first 512 bytes
// usually programs start @ 0x200 & don't access <0x200
// upper 256 0xF00 - 0xFFF reserved for display refresh
// 96 bytes below that 0xEA0 - 0xEFF call stack etc.
// modern chip 8 has external intepreter and first
// 512 are used to store font information

unsigned char display[64*32];

//display is 64x32 pixels (bit coded), drawn only wiht sprites
// 8 pixels wide and 1-15 pixels in height
//pixels that are set, just flip the color of the screen pixels
//unset pixels do nothing
//VF is set to 1 if any pixels are flipped from set to unset, otherwise 0
//collision detection

unsigned char hexChars[] = {
              0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
              0x20, 0x60, 0x20, 0x20, 0x70, // 1
              0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
              0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
              0x90, 0x90, 0xF0, 0x10, 0x10, // 4
              0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
              0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
              0xF0, 0x10, 0x20, 0x40, 0x40, // 7
              0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
              0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
              0xF0, 0x90, 0xF0, 0x90, 0x90, // A
              0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
              0xF0, 0x80, 0x80, 0x80, 0xF0, // C
              0xE0, 0x90, 0x90, 0x90, 0xE0, // D
              0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
              0xF0, 0x80, 0xF0, 0x80, 0x80 // F
          };

unsigned int keymap[0x10] = {
    SDL_SCANCODE_0,
    SDL_SCANCODE_1,
    SDL_SCANCODE_2,
    SDL_SCANCODE_3,
    SDL_SCANCODE_4,
    SDL_SCANCODE_5,
    SDL_SCANCODE_6,
    SDL_SCANCODE_7,
    SDL_SCANCODE_8,
    SDL_SCANCODE_9,
    SDL_SCANCODE_A,
    SDL_SCANCODE_B,
    SDL_SCANCODE_C,
    SDL_SCANCODE_D,
    SDL_SCANCODE_E,
    SDL_SCANCODE_F
};

const unsigned char* keys;

register V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, VA, VB, VC, VD, VE, VF = 0;

register registry[16];
// = { V0, V1, V2, V3, V4, V5, V6, V7, V8, V9, VA, VB, VC, VD, VE, VF };

//address register is 16 bit
unsigned short addr_register;


//48 bytes for up to 12 levels of nesting
// modern implementations have 16 levels
unsigned short stack[16];


//chip-8 has 2 timers both count down at 60hz
//until they reach 0
//delay timer intended for timing events,
//it can be set and read
//sound timer used for sound effects.
//when its nonzero a beeping sound is made

unsigned char delay_timer = 0;
unsigned char sound_timer = 0;


//program counter is a pseudo-register
unsigned short PC = 0;
unsigned char SP = 0;
unsigned short opcode = 0;

//input is a hex keyboard (16) from 0-F
// 8, 4, 6, 2 are typically for direction
const unsigned char* keyPressed(){
  //printf("key is: %x\n", key);
  keys = SDL_GetKeyboardState(NULL);
  return keys;
}

void clear_display(){
  for(int i = 0; i < 0x100; i++)
  {
    //memory[0xF00 + i] = 0;
  }
  for(int i = 0; i < 64*32; i++)
  {
    display[i] = 0;
  }
}

void loadgame(const char* filename){
  FILE* file = fopen(filename, "r");
  int val = fgetc(file);
  short memaddress = 0x200;
  while(val != EOF){
    memory[memaddress++] = (unsigned char)val;
    val = fgetc(file);
  }
   fclose(file);
   //printf("hey this is the filesize, check it %d\n", memaddress-0x200);
}

void load_fonts(){
  //load hex character fonts
  for( unsigned char i = 0; i < 80; i++)
    memory[i] = hexChars[i];
}

//35 opscodes are all 2 bytes long and stored
// big-endian
void execute_opcode(unsigned short opcode){
  //going to make a switch to execute all opscodes
  //depending on what's passed in

  //printf("opcode: %x\n", opcode);
  unsigned char x = (opcode & 0x0F00) >> 8;
  unsigned char y = (opcode & 0x00F0) >> 4;
  unsigned char val, N;
  unsigned int j = 0;

  //printf("Cycle Mem addy: %x\t Cycle Mem contents: %x\t X: %x\t Y: %x\t I: %x\t N: %d\t mem[I]: %x\t SP: %x\t stack[SP]: %x\n", PC, opcode, x, y, addr_register, N, memory[addr_register], SP, stack[SP]);
  //printf("V0: %x V1: %x V2: %x V3: %x V4: %x V5: %x V6: %x V7: %x V8: %x V9: %x VA: %x VB: %x VC: %x VD: %x VE: %x VF: %x\n", registry[0], registry[1], registry[2],
  //  registry[3], registry[4], registry[5], registry[6], registry[7], registry[8], registry[9], registry[0xA], registry[0xB], registry[0xC], registry[0xD], registry[0xE], registry[0xF]);

  //printf("gameaddy: %x\t PC: %x\t mem[PC]: %x\t mem[PC + 1]: %x\t opcode: %x\n", PC-0x200, PC, memory[PC], memory[PC + 1], opcode);

  //find out what the first 4bits are
  switch(opcode & 0xF000){


    case 0x0000:
      //first 4bits are zeros
      switch(opcode){
        //clear the screen
        case 0x00E0:
          clear_display();
          PC += 2;
        break;

        //return from a subroutine
        case 0x00EE:
          PC = stack[--SP] + 2;
        break;

        //0NNN
        default:
          printf("you shouldn't be here. Opcode: %04X", opcode);
        break;
      }
    break;

    case 0x1000:
      //1NNN
      //jump to address NNN
      PC = opcode & 0x0FFF;
    break;

    case 0x2000:
      //2NNN
      //calls subroutine at NNN
      stack[SP++] = PC;
      PC = opcode & 0x0FFF;
    break;

    case 0x3000:
      if(registry[x] == (opcode & 0x00FF))
        PC += 4;
      else
        PC += 2;
    break;

    case 0x4000:
      if(registry[x] != (opcode & 0x00FF))
        PC += 4;
      else
        PC += 2;
    break;

    case 0x5000:
      if(registry[x] == registry[y])
        PC += 4;
      else
        PC += 2;
    break;

    case 0x6000:
      registry[x] = opcode & 0x00FF;
      PC += 2;
    break;

    case 0x7000:
      registry[x] += opcode & 0x00FF;
      PC += 2;
    break;

    case 0x8000:
      switch(opcode & 0x000F){

        case 0x0:
          registry[x] = registry[y];
          PC += 2;
        break;

        case 0x1:
          registry[x] = registry[x] | registry[y];
          registry[0xF] = 0;
          PC += 2;
        break;

        case 0x2:
          registry[x] = registry[x] & registry[y];
          registry[0xF] = 0;
          PC += 2;
        break;

        case 0x3:
          registry[x] = registry[x] ^ registry[y];
          registry[0xF] = 0;
          PC += 2;
        break;

        case 0x4:
          if((int)((int)registry[y] + (int)registry[x]) < 256)
          {
            registry[0xF] = 0;
          }
          else
          {
            registry[0xF] = 1;
          }
          registry[x] += registry[y];
          PC += 2;
        break;

        case 0x5:
          if((int)((int)registry[x] - (int)registry[y]) >= 0)
          {
            registry[0xF] = 1;
          }
          else
          {
            registry[0xF] = 0;
          }
          registry[x] -= registry[y];
          PC += 2;
        break;

        case 0x6:
          registry[0xF] = registry[opcode & 0x01];
          registry[x] = registry[x] >> 1;
          PC += 2;
        break;

        case 0x7:
          if((registry[y] - registry[x]) > 0 )
          {
            registry[0xF] = 1;
          }
          else
          {
            registry[0xF] = 0;
          }

          registry[x] = registry[y] - registry[x];
          PC += 2;
        break;

        case 0xE:
          registry[0xF] = (registry[x] & 0x80) >> 7;
          registry[x] = registry[x] << 1;
          PC += 2;
        break;

        default:
          printf("you shouldn't be here. Opcode: %04X", opcode);
        break;

      }
    break;

    case 0x9000:
      if(registry[x] != registry[y])
        PC += 4;
      else
        PC += 2;
    break;

    case 0xA000:
      addr_register = opcode & 0x0FFF;
      PC += 2;
    break;

    case 0xB000:
      PC = registry[0] + (opcode & 0x0FFF);
    break;

    case 0xC000:
      registry[x] = (rand() % 256) & (opcode & 0x00FF);
      PC += 2;
    break;

    case 0xD000:
        N = opcode & 0x000F;
        //printf("Cycle Mem addy: %x\t Cycle Mem contents: %x\t X: %x\t Y: %x\t I: %x\t N: %d\t mem[I]: %x\t SP: %x\t stack[SP]: %x\n", PC, opcode, x, y, addr_register, N, memory[addr_register], SP, stack[SP]);
        //printf("V0: %x V1: %x V2: %x V3: %x V4: %x V5: %x V6: %x V7: %x V8: %x V9: %x VA: %x VB: %x VC: %x VD: %x VE: %x VF: %x\n", registry[0], registry[1], registry[2],
        //  registry[3], registry[4], registry[5], registry[6], registry[7], registry[8], registry[9], registry[0xA], registry[0xB], registry[0xC], registry[0xD], registry[0xE], registry[0xF]);
        drawflag = 0;

        registry[0xF] = 0;
      for(unsigned char i = 0; i < N; i++){
        unsigned char byteoffset = (64 * registry[y] + registry[x])/8;

        pixel = memory[addr_register + i];
        //printf("Mem addy: %x\t Mem contents: %x\n", addr_register + i, memory[addr_register + i]);
        for(unsigned char col = 0; col < 8; col++){
          //printf("PIXEL: %x\t CornerX: %d CornerY: %d\t FLAG: %x\t", pixel, x + col, y + i, (pixel & (0x80 >> col)));
          if((pixel & (0x80 >> col)) != 0)
          {
            //if((((y+i)*PPR) + x + col) >= 64*32)
            //  continue;
            //if set to unset then set VF flag to 1
            if(display[((registry[y]+i)*PPR) + registry[x] + col] != 0)
            {
              registry[0xF] = 1;
            }
            //printf("DrawX: %d\t DrawY: %d\t PIX: %x\n", x + col, y+i, display[((y+i)*PPR) + x + col]);
            display[((registry[y]+i)*PPR) + registry[x] + col] ^= 1;
            drawflag = 1;
          } else {
            //printf("\n");
          }
        }
      }

      PC += 2;
    break;

    case 0xE000:

      switch(opcode & 0x00FF){

        case 0x9E:
          keyPressed();
          if(keys[keymap[registry[x]]])
            PC += 4;
          else
            PC += 2;
        break;

        case 0xA1:
          keyPressed();
          if(!keys[keymap[registry[x]]])
            PC += 4;
          else
            PC += 2;
        break;

        default:
          printf("you shouldn't be here. Opcode: %04X", opcode);
        break;

      }
    break;

    case 0xF000:
      switch(opcode & 0x00FF){

        case 0x07:
          registry[x] = delay_timer;
          PC += 2;
        break;

        case 0x0A:
          while(1){
            keyPressed();
            if(keys[keymap[registry[j]]]){
              registry[x] = j;
              PC += 2;
              break;
            } else {
              j = (j + 1) % 0x10;
            }
          }
        break;

        case 0x15:
          delay_timer = registry[x];
          PC += 2;
        break;

        case 0x18:
          sound_timer = registry[x];
          PC += 2;
        break;

        case 0x1E:
          addr_register += registry[x];
          PC += 2;
        break;

        case 0x29:
          addr_register = hexfont * x;
          PC += 2;
        break;

        case 0x33:
          memory[addr_register] = registry[x] / 100;
          memory[addr_register + 1] = (registry[x] % 100) / 10;
          memory[addr_register + 2] = registry[x] % 10;
          PC += 2;
        break;

        case 0x55:
          for (short i = 0; i <= x; i++ ){
            memory[addr_register + i] = registry[i];
          }
          PC += 2;
        break;

        case 0x65:
          for (short i = 0; i <= x; i++ ){
            registry[i] = memory[addr_register + i];
          }
          PC += 2;
        break;

        default:
          printf("you shouldn't be here. Opcode: %04X", opcode);
        break;
      }
    break;

    default:
      printf("you shouldn't be here. Opcode: %04X", opcode);
    break;
  }
}

void initialize(){
  PC = 0x200;
  load_fonts();
  clear_display();
  loadgame("./c8games/TETRIS");
}

void reset(){
  for(int i = 0; i< 0x1000; i++){
    memory[i] = 0;
  }
}

void updateTimers(){
  //sleep(3);
  //usleep(166666);
  //usleep(83333);
  //usleep(41666);
  //usleep(20833);
  //usleep(10416);
  //usleep(5208);
  //usleep(2604);
  if(sound_timer > 0){
    sound_timer--;
  }

  if(delay_timer > 0){
    delay_timer--;
  }

  if(sound_timer != 0){
    //printf("sound\n");
    //printf("\a");
  }
}

void getKeys(){
  /* Poll for events. SDL_PollEvent() returns 0 when there are no  */
  /* more events on the event queue, our while loop will exit when */
  /* that occurs.                                                  */
  SDL_Event event;
  while( SDL_PollEvent( &event ) ){
    /* We are only worried about SDL_KEYDOWN and SDL_KEYUP events */
    switch( event.type ){
      case SDL_KEYDOWN:
        //printf( "Key press detected\n" );
        switch(event.key.keysym.scancode){
          case SDL_SCANCODE_ESCAPE:
            printf("Exiting...\n");
            booty = 0;
          break;
          case SDL_SCANCODE_TAB:
            printf("RESET\n");
            reset();
            initialize();
            //booty = 0;
          break;
          default:
          break;
        }
      break;

      case SDL_KEYUP:
        //printf( "Key release detected\n" );
        break;

      default:
        break;
    }
  }
}

void cycle(){

  //()
  //load opcode
  opcode = (memory[PC] << 8) | memory[PC + 1];
  //printf("Cycle Mem addy: %d\t Cycle Mem contents: %x\n", PC, opcode);
  //printf("Cycle Mem addy: %d\t Cycle Mem contents: %x\t V0 = %x V1 = %x V2 = %x V3 = %x StackPointer = %d\n", PC, opcode, registry[0], registry[1], registry[2], registry[3], SP);

  execute_opcode(opcode);
  //PC += 2;
}

void updateDisplay(){
  if(drawflag){
    for(int y=0; y < 0x20; y++){
      for(int x=0; x < 0x40; x++){
        //location = y*ppr + x
        unsigned char shift = (x%8);
        unsigned char byteoffset = (64 * y + x)/8;

        //rectangle = {x, y, 10, 10};
        rectangle.x = x * SCALEFACTOR;
        rectangle.y = y * SCALEFACTOR;
        //rectangle.x = 630;
        //rectangle.y = 300;
        rectpointer = &rectangle;
        // & (0x80 >> shift)
        if(display[x + (y*PPR)]){
          SDL_FillRect( screenSurface, rectpointer, SDL_MapRGB( screenSurface->format, 0x00, 0x00, 0xFF ) );
          //printf("\tDisplayX: %d\t DisplayY: %d\t PIX: %x\n", x, y, display[x + (y*PPR)]);
          //printf("Mem addy: %x\t Mem contents: %x\n", 0xF00 + byteoffset, memory[0xF00 + byteoffset]);
        }
        else{
          SDL_FillRect( screenSurface, rectpointer, SDL_MapRGB( screenSurface->format, 0x00, 0xFF, 0x00 ) );
        }
        //printf("\tDisplayX: %d\t DisplayY: %d\n", x, y);
      }
    }
    //Update the surface
    SDL_UpdateWindowSurface( window );
    //drawflag = 0;
  }
}

void printMem(){
  for(int i = 0; i < 0x1000; i++){
    printf("Mem Address: %x\t\t\t Value: %x\n", i, memory[i]);
    //printf("\n\n\n\n\n\n\n\n\n\n");
    //printf("\n\n\n\n\n\n\n\n\n\n");
    //printf("\n\n\n\n\n\n\n\n\n\n");
    //printf("\n\n\n\n\n\n\n\n\n\n");
    //printf("\n\n\n\n\n\n\n\n\n\n");
    //sleep(2);
  }
}

int main(int argc, char *argv[]){
  reset();
  initialize();
  //printMem();

  //The window we'll be rendering to
	window = NULL;

	//The surface contained by the window
	screenSurface = NULL;

	//Initialize SDL
	if( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0 )
	{
		printf( "SDL could not initialize! SDL_Error: %s\n", SDL_GetError() );
	}
	else
	{
		//Create window
		window = SDL_CreateWindow( "SDL Tutorial", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN );
		if( window == NULL )
		{
			printf( "Window could not be created! SDL_Error: %s\n", SDL_GetError() );
		}
		else
		{
			//Get window surface
			screenSurface = SDL_GetWindowSurface( window );

			//Fill the surface white
			SDL_FillRect( screenSurface, NULL, SDL_MapRGB( screenSurface->format, 0xFF, 0xFF, 0xFF ) );

			//Update the surface
			SDL_UpdateWindowSurface( window );

      while(booty != 0){
        updateTimers();
        getKeys();
        cycle();
        updateDisplay();
      }
		}
	}

	//Destroy window
	SDL_DestroyWindow( window );

	//Quit SDL subsystems
	SDL_Quit();

  return 0;
}
