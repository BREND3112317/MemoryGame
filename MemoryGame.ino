#include <FastLED.h>
#include <time.h>

// WCMCU-2812B-16 leds count
#define NUM_LEDS 16

// the MAX7219 address map (datasheet table 2)
#define MAX7219_DECODE_REG      (0x09)
#define MAX7219_INTENSITY_REG   (0x0A)
#define MAX7219_SCANLIMIT_REG   (0x0B)
#define MAX7219_SHUTDOWN_REG    (0X0C)
#define MAX7219_DISPLAYTEST_REG (0x0F)
#define MAX7219_DIGIT_REG(pos)  ((pos) + 1)
#define MAX7219_COLUMN_REG(pos) MAX7219_DIGIT_REG(pos)
#define MAX7219_NOOP_REG        (0x00)

// shutdown mode (datasheet table 3)
#define MAX7219_OFF             (0x0)
#define MAX7219_ON              (0x1)

const int clock_pin = 9; //CLK
const int data_latch_pin = 10; //LOAD
const int data_input_pin = 11; //DIN

// number of columns of the display matrx
#define NUM_OF_COLUMNS  (8)
// for each character bitmap, it consumes 4 bytes
#define BYTE_PER_MAP    (8)
// define the number of chained matrixes
#define NUM_OF_MATRIXES (4)

const byte char_pattern[] =
{
  B00011100, B00100010, B00100010, B00100010, B00100010, B00100010, B00011100, B00000000, // 0
  B00010000, B00010000, B00010000, B00010000, B00010100, B00011000, B00010000, B00000000, // 1
  B00111110, B00000100, B00001000, B00010000, B00100000, B00100010, B00011100, B00000000, // 2
  B00011100, B00100010, B00100000, B00011000, B00100000, B00100010, B00011100, B00000000, // 3
  B00010000, B00010000, B00011111, B00010010, B00010100, B00011000, B00010000, B00000000, // 4
  B00011110, B00100000, B00100000, B00011110, B00000010, B00000010, B00111110, B00000000, // 5
  B00011100, B00100010, B00100010, B00011110, B00000010, B00100010, B00011100, B00000000, // 6
  B00001000, B00001000, B00001000, B00010000, B00100000, B00100000, B00111110, B00000000, // 7
  B00011100, B00100010, B00100010, B00011100, B00100010, B00100010, B00011100, B00000000, // 8
  B00011100, B00100010, B00100000, B00111100, B00100010, B00100010, B00011100, B00000000, // 9
};

#define DISPLAY_STR_LENGTH  (sizeof(char_pattern) / BYTE_PER_MAP)

// MAX7219 basic
void set_all_registers(byte, byte);
void set_single_register(int, byte, byte);
void init_max7219();

// program function
void clearAllDisplayMatrx();
void hashdisplay();
void displaySuccess();
void displayError();
void GameOver();
void show_simple_leds(CRGB val);

// LED Parameter
CRGB leds[NUM_LEDS];
unsigned int led_color = 0;
unsigned int led_index = 0;
unsigned int led_MaxLevel = 16;

// Game Parameter
const int ply_MaxLevel = 9; //2~9
const int ply_MinLevel = 2; //2~9
int ply_level = ply_MinLevel; // 2~9 關卡
int display[ply_MaxLevel] = {};
const int ply_delay[] = {1000, 950, 900, 850, 800, 750, 700, 650}; // 關卡速度
int lv_rank = 0; // (每次)遊戲機分
int GameBreak = 0;

// input Parameter
int inVal[4] = {0}, valimit=100;
int enable=0, e_index=-1;
int val = 100;


// update a specific register value of all MAX7219s
void set_all_registers(byte address, byte value)
{
  digitalWrite(data_latch_pin, LOW);

  for (int i = 0; i < NUM_OF_MATRIXES; i++)
  {
    shiftOut(data_input_pin, clock_pin, MSBFIRST, address);
    shiftOut(data_input_pin, clock_pin, MSBFIRST, value);
  }
  
  digitalWrite(data_latch_pin, HIGH);
}

// only update the register in one MAX7219
void set_single_register(int index, byte address, byte value)
{
  // only process for valid index range
  if (index >= 0 && index < NUM_OF_MATRIXES)
  {
    digitalWrite(data_latch_pin, LOW);

    for (int i = NUM_OF_MATRIXES - 1; i >= 0; i--)
    {
      // for specified MAX7219, access the desired register
      if (i == index)
      {
        shiftOut(data_input_pin, clock_pin, MSBFIRST, address);
    
      }
      else
      {
        // send No-Op operation to all other MAX7219s (the value is "don't-care" for No-Op command)
        shiftOut(data_input_pin, clock_pin, MSBFIRST, MAX7219_NOOP_REG);
      }
      
      shiftOut(data_input_pin, clock_pin, MSBFIRST, value);
    }
  
    digitalWrite(data_latch_pin, HIGH);
  }
}

void init_max7219()
{
  // disable test mode. datasheet table 10
  set_all_registers(MAX7219_DISPLAYTEST_REG, MAX7219_OFF);
  // set medium intensity. datasheet table 7
  set_all_registers(MAX7219_INTENSITY_REG, 0x0);
  // turn off display. datasheet table 3
  set_all_registers(MAX7219_SHUTDOWN_REG, MAX7219_OFF);
  // drive 8 digits. datasheet table 8
  set_all_registers(MAX7219_SCANLIMIT_REG, 7);
  // no decode mode for all positions. datasheet table 4
  set_all_registers(MAX7219_DECODE_REG, B00000000);

  // clear matrix display
  clearAllDisplayMatrx();
}

void clearAllDisplayMatrx(){
  set_all_registers(MAX7219_SHUTDOWN_REG, MAX7219_OFF);
  for(int i=0;i<NUM_OF_MATRIXES;++i)
    for(int j=0;j<BYTE_PER_MAP;++j)
      set_single_register(i, MAX7219_COLUMN_REG(0 + j), B00000000);
  set_all_registers(MAX7219_SHUTDOWN_REG, MAX7219_ON);
}

void hashdisplay(){
  srand(analogRead(A5));
  int i=0, pos=0, tmp;
  for(i=0;i<ply_MaxLevel;++i)display[i]=i%NUM_OF_MATRIXES;
  for(i=0;i<ply_MaxLevel;++i){
    pos = ply_MaxLevel*(double)rand()/RAND_MAX;
    tmp = display[i];
    display[i] = display[pos];
    display[pos] = tmp;
  }
}

void displaySuccess() {
  show_simple_leds(CHSV(85,255,100)); 
  delay(100);
  show_simple_leds(CRGB::Black); 
  delay(100);
  show_simple_leds(CHSV(85,255,100)); 
  delay(1000);
  show_simple_leds(CRGB::Black); 
  delay(100);
}

void displayError() {
  show_simple_leds(CHSV(0,255,100)); 
  delay(100);
  show_simple_leds(CRGB::Black); 
  delay(100);
  show_simple_leds(CHSV(0,255,100)); 
  delay(1000);
  show_simple_leds(CRGB::Black); 
  delay(100);
}

void show_simple_leds(CRGB val) {
  for(int i=0;i<NUM_LEDS;++i)leds[i] = val;
  FastLED.show();
}

void GameOver() {
  while(ply_level>9) {
    leds[(led_index/4)%16] = CHSV(led_color%255,255,50); 
    led_color+=3;
    ++led_index;
    FastLED.show(); delay(10);
  }
}

void setup()  
{
  Serial.begin(9600);
  
  // input states
  pinMode(A0, INPUT);
  pinMode(A1, INPUT);
  pinMode(A2, INPUT);
  pinMode(A3, INPUT);
  pinMode(A5, INPUT);

  // init LED states
  FastLED.addLeds<NEOPIXEL,6>(leds, NUM_LEDS); 

  // init pin states
  pinMode(clock_pin, OUTPUT);
  pinMode(data_latch_pin, OUTPUT);    
  pinMode(data_input_pin, OUTPUT);

  // init MAX2719 states
  init_max7219();
}

void loop()  
{
  if(ply_level>9) GameOver();

  // 初始化
  clearAllDisplayMatrx();
  hashdisplay();
  int i, j, t = 100;
  lv_rank=0;
  GameBreak = 0;
  enable = 0;
  e_index = -1;
  
  // 開始遊戲
  show_simple_leds(CHSV(150,255,50)); 
  Serial.print("Game Start! Level ");
  Serial.println(ply_level);
  Serial.print("Speed: ");
  Serial.println(ply_delay[ply_level-2]);

  // 顯示答案
  Serial.println( "Answer:" );
  for(i=0;i<ply_level;++i){
    // turn off display first
    set_all_registers(MAX7219_SHUTDOWN_REG, MAX7219_OFF);
    // display one bitmap
    for (j = 0; j < BYTE_PER_MAP; j++)
    { 
      // starting from column 2
      set_single_register(display[i], MAX7219_COLUMN_REG(0 + j), char_pattern[(i+1) * BYTE_PER_MAP + j]);
    }
    // turn on display
    set_all_registers(MAX7219_SHUTDOWN_REG, MAX7219_ON);
    delay(ply_delay[ply_level-2]);
    clearAllDisplayMatrx();
    Serial.print( display[i] );
  }
  Serial.println( "" );

  // 作答
  Serial.println( "Please input your ans:" );
  while(lv_rank<ply_level && GameBreak==0){
    // 讀值
    inVal[0] = analogRead(A0);
    inVal[1] = analogRead(A1);
    inVal[2] = analogRead(A2);
    inVal[3] = analogRead(A3);
    // 傳送資料至電腦

    for(int i=0;i<4;++i){
      if(GameBreak)break;
      if(inVal[i]<valimit){
        e_index = i;
        enable = 1;
      }else if(enable && (e_index>=0 && inVal[e_index]>=1.3*valimit)) {
        Serial.print("Click ");
        Serial.print( e_index );
        Serial.print( ": " );
        Serial.println(inVal[e_index]);

        if(display[lv_rank] == e_index){
          ++lv_rank;
          enable = 0;
          e_index = -1;
          if(lv_rank==ply_level){
            Serial.println( "\tSuccess" );
            displaySuccess();
            ++ply_level;
            GameBreak = 1;
            break;
          }
        }else{
          Serial.println( "\tError" );
          displayError();
          ply_level = ply_MinLevel;
          GameBreak = 1;
          break;
        }
      }
    }

    // 幻彩燈(階級)
    for(i=0;i<(led_MaxLevel-7/3*(ply_level-2));++i){
      leds[((led_MaxLevel-7/3*(ply_level-2))+led_index/4-i)%16] = CHSV(led_color%255,255,50); 
    }
    led_color+=3;
    ++led_index;
    FastLED.show(); delay(10);
  }
  
}
