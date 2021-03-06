#include <Arduino.h>

// dimensions of the playground
#define HEIGHT 16
#define WIDTH 8

// what a color value in playground means:
#define RED 1
#define GREEN 2
#define BLUE 3
#define YELLOW 4
#define FUCHSIA 5
#define AQUA 6
#define BLINK -1

// sets a bit of an uint8_t to a desired value
void setBit(uint8_t &_byte, uint8_t _bit, bool value) {
  if (value)
    _byte |= (1 << _bit);
  else
    _byte &= ~(1 << _bit);
}

// toggles a bit of an uint8_t
void toggleBit(uint8_t &_byte, uint8_t _bit) { _byte ^= (1 << _bit); }

// returns the value of a bit of an uint8_t
bool getBit(uint8_t &_byte, uint8_t _bit) { return _byte & (1 << _bit); }

static uint8_t playground[WIDTH][HEIGHT]; // playing array
static int8_t tile[4][2];    // storage for 4 block coordinates for current tile
static int8_t pos[2];        // position of pivot point of current tile
static uint8_t tile_nr = 0;  // type of current tile  (index)
static uint8_t rotation = 0; // rotation of current tile (sub_index)
static uint8_t status =
    0b10000000;             // {reset|pause|rotCCW|rotCW|left|right|down|timer}
static uint8_t color = RED; // color value of the current tile

// relative positions of the three remaining blocks
// relative to the pivot point
static const int8_t tiles[][3][2] = {
    {{0, -2}, {0, -1}, {0, 1}},  // long piece vert
    {{-1, 0}, {1, 0}, {2, 0}},   // long piece hori
    {{1, 0}, {0, 1}, {1, 1}},    // block
    {{-1, 0}, {0, 1}, {1, 1}},   // stairs down hori
    {{0, -1}, {-1, 0}, {-1, 1}}, // stairs down vert
    {{1, 0}, {0, 1}, {-1, 1}},   // stairs up hori
    {{-1, -1}, {-1, 0}, {0, 1}}, // stairs up vert
    {{0, -1}, {0, 1}, {-1, 1}},  // L left 000
    {{-1, -1}, {-1, 0}, {1, 0}}, // L left 090
    {{0, -1}, {1, -1}, {0, 1}},  // L left 180
    {{-1, 0}, {1, 0}, {1, 1}},   // L left 270
    {{0, -1}, {0, 1}, {1, 1}},   // L right 000
    {{-1, 1}, {-1, 0}, {1, 0}},  // L right 090
    {{0, -1}, {-1, -1}, {0, 1}}, // L right 180
    {{-1, 0}, {1, 0}, {1, -1}},  // L right 270
    {{-1, 0}, {0, -1}, {1, 0}},  // pedestral 000
    {{0, -1}, {1, 0}, {0, 1}},   // pedestral 090
    {{-1, 0}, {0, 1}, {1, 0}},   // pedestral 180
    {{-1, 0}, {0, -1}, {0, 1}}   // pedestral 270
};

// gets the specified tile
// writes it into the tile variable
void getTile(const uint8_t tile_nr, const uint8_t tile_rot, const int8_t x,
             const int8_t y) {
  size_t _index = 0;
  switch (tile_nr) {
  case 0:
    _index = tile_rot % 2;
    break; // long piece
  case 1:
    _index = 2;
    break; // block
  case 2:
    _index = 3 + tile_rot % 2;
    break; // stairs up
  case 3:
    _index = 5 + tile_rot % 2;
    break; // stairs down
  case 4:
    _index = 7 + tile_rot % 4;
    break; // L left
  case 5:
    _index = 11 + tile_rot % 4;
    break; // L right
  case 6:
    _index = 15 + tile_rot % 4;
    break; // pedestral
  }
  tile[0][0] = x;
  tile[0][1] = y;
  for (size_t i = 0; i < 3; i++) {
    tile[i + 1][0] = x + tiles[_index][i][0];
    tile[i + 1][1] = y + tiles[_index][i][1];
  }
}

// checks if current tile would overwrite existing blocks
// returns true if collision is detected
bool checkCollision() {
  for (size_t i = 0; i < 4; i++) {
    int8_t _x = tile[i][0];
    int8_t _y = tile[i][1];

    if (_x < 0 || _x >= WIDTH || _y >= HEIGHT ||
        (_y >= 0 && playground[_x][_y] != 0)) {
      return true;
    }
  }
  return false;
}

// tries to rotate clockwise (1) or CCW (-1)
void rotate(const int8_t dir) {
  getTile(tile_nr, (rotation + dir + 4) % 4, pos[0], pos[1]);
  if (checkCollision()) {
    getTile(tile_nr, rotation, pos[0], pos[1]);
  } else {
    rotation = (rotation + 4 + dir) % 4;
  }
}

// tries to move one block left (-1) or right (1)
void translate(const int8_t dir) {
  for (size_t i = 0; i < 4; i++) {
    tile[i][0] += dir;
  }
  if (checkCollision()) {
    for (size_t i = 0; i < 4; i++) {
      tile[i][0] -= dir;
    }
  } else {
    pos[0] += dir;
  }
}

// tries to move one block down
// returns false if it fails
bool fall_down() {
  for (size_t i = 0; i < 4; i++) {
    tile[i][1]++;
  }
  if (checkCollision()) {
    for (size_t i = 0; i < 4; i++) {
      tile[i][1]--;
    }
    return false;
  } else {
    pos[1]++;
    return true;
  }
}

// creates new tile
void resetTile() {
  pos[0] = WIDTH / 2;
  pos[1] = -1;
  tile_nr = random(7);
  rotation = random(4);
  color = random(6) + 1;
  getTile(tile_nr, rotation, pos[0], pos[1]);
}

// oerwrites playground with zeros
void clearPG() {
  for (size_t i = 0; i < WIDTH; i++) {
    for (size_t j = 0; j < HEIGHT; j++) {
      playground[i][j] = 0;
    }
  }
}

// draw the tile to the screen
// returns false, if tile sticks out of the screen
bool applyTile() {
  bool result = true;
  for (size_t i = 0; i < 4; i++) {
    int8_t _x = tile[i][0];
    int8_t _y = tile[i][1];
    if (_y >= 0) {
      playground[_x][_y] = static_cast<uint8_t>(color);
    } else
      result = false;
  }
  return result;
}

// erases tile from the screen
void unapplyTile() {
  for (size_t i = 0; i < 4; i++) {
    int8_t _x = tile[i][0];
    int8_t _y = tile[i][1];
    if (_y >= 0) {
      playground[_x][_y] = 0;
    }
  }
}

// finds completed lines and marks them
void checkLines() {
  for (size_t j = 0; j < HEIGHT; j++) {
    size_t i = 0;
    while (i < WIDTH - 1) {
      if (playground[i][j] == 0) {
        break;
      }
      i++;
    }
    if (i == WIDTH - 1 && playground[i][j] != 0) {
      for (i = 0; i < WIDTH; i++) {
        playground[i][j] = static_cast<uint8_t>(BLINK);
      }
    }
  }
}

// collapses marked lines
void removeLines() {
  for (size_t k = 0; k < HEIGHT; k++) {
    if (playground[0][k] == static_cast<uint8_t>(BLINK)) {
      for (size_t i = 0; i < WIDTH; i++) {
        for (size_t j = k; j > 0; j--) {
          playground[i][j] = playground[i][j - 1];
        }
      }
      for (size_t i = 0; i < WIDTH; i++) {
        playground[i][0] = 0;
      }
    }
  }
}
// performs one game logic tick
void tick() {
  // status = {7...0} = {reset|pause|rotCCW|rotCW|left|right|down|timer}
  if (getBit(status, 6)) {
    if (getBit(status, 7)) {
      for (size_t j = HEIGHT - 2; j >= 0; j--) {
        if (playground[0][j] != static_cast<uint8_t>(BLINK) &&
            playground[0][j + 1] == static_cast<uint8_t>(BLINK)) {
          for (size_t i = 0; i < WIDTH; i++) {
            playground[i][j] = static_cast<uint8_t>(BLINK);
          }
        }
      }
    }
  } else if (getBit(status, 7)) {
    setBit(status, 7, false);
    resetTile();
    clearPG();
  } else {
    unapplyTile();
    if (getBit(status, 5)) {
      setBit(status, 5, false);
      rotate(-1);
    }
    if (getBit(status, 4)) {
      setBit(status, 4, false);
      rotate(1);
    }
    if (getBit(status, 3)) {
      setBit(status, 3, false);
      translate(-1);
    }
    if (getBit(status, 2)) {
      setBit(status, 2, false);
      translate(1);
    }
    if (getBit(status, 0)) {
      removeLines();
    }
    if (getBit(status, 1) || getBit(status, 0)) {
      setBit(status, 0, false);
      setBit(status, 1, false);
      if (!fall_down()) {
        if (!applyTile()) { // you lost :o
          setBit(status, 7, true);
          setBit(status, 6, true);
          for (size_t i = 0; i < WIDTH; i++) {
            playground[i][HEIGHT - 1] = static_cast<uint8_t>(BLINK);
          }
        } else {
          resetTile();
          checkLines();
        }
      }
    }
    applyTile();
  }
}

volatile int x;
int steps;

ISR( TIMER1_OVF_vect  ) { //update display
  //playground[x][y]

  PORTC = 0x00;
  PORTD = 0x00;

  PORTA = ~(1<<x);

  for(int y = 0; y < 8 ; y++)
  {
    if(playground[x][y] >= 1)
      PORTC |= (1<<y);
    if(playground[x][y+8] >= 1)
      PORTD |= (1<<y);
  }

  if(x<7)
    x++;
  else
    x = 0;
}


void setup()
{
  TCCR1B  = (1<<CS11); //| (1<<CS20); // Prescaler 8
  TIMSK |= (1<<TOIE1);            // Timer Overflow Interrupt freischalten
  tick();
  resetTile();
  sei(); //enable interrupt
  x = 0;
  steps = 0;
  //SET PORTS OUTPUT AND INPUT
  DDRA = 0xFF;
  DDRC = 0xFF;
  DDRD = 0xFF;
  DDRB = 0b11111000;
  PORTB = 0b00000111; // pullup
}



void loop()
{

  delay(30);
  if(steps >= 5)
  {
    status |= 0b00000001;
    tick();
    steps = 0;
  }

  //unschön geschriebene button abfrage

  if((PINB |= 0b11111110) == 0b11111110)
    status |= 0b00010000; //turn cw
  if((PINB |= 0b11111011) == 0b11111011)
    status |= 0b00001000; //shift left
  if((PINB |= 0b11111101) == 0b11111101)
    status |= 0b00000100; //shift right

  tick();
  steps++;
}
