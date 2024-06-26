#include "driver/ledc.h" //for ay clock

#define AY_CLK       15
// HC595 define
#define OUT_SHIFT_DATA_PIN  0 // -> pin 14 of 74HC595 - data input, slave in SI (DS)
#define OUT_SHIFT_LATCH_PIN 2 // -> pin 12 of 74HC595 - data output latch (ST_CP)
#define OUT_SHIFT_CLOCK_PIN 4 // -> pin 11 of 74HC595 - clock pin SCK (SH_CP)
//2nd hc595 control bits
#define BIT_AY_RESET 0
#define BIT_AY_BC1   1
#define BIT_AY_BDIR1 2
#define BIT_AY_BDIR2 3

hw_timer_t* AYTimer=NULL;

byte outLo=0,outHi=0;
int skipCnt=0;
bool writeFlag=true;

const uint32_t ay_channel_remap_table[6*16]={
  0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15, //LAY_ABC
  0,1,4,5,2,3,6,7,8,10,9,11,12,13,14,15, //LAY_ACB
  2,3,0,1,4,5,6,7,9,8,10,11,12,13,14,15, //LAY_BAC
  2,3,4,5,0,1,6,7,9,10,9,11,12,13,14,15, //LAY_BCA
  4,5,0,1,2,3,6,7,10,8,9,11,12,13,14,15, //LAY_CAB
  4,5,2,3,0,1,6,7,10,9,8,11,12,13,14,15, //LAY_CBA
};

const uint32_t ay_mixer_remap_table[6*3]={
  0,1,2, //LAY_ABC
  0,2,1, //LAY_ACB,
  1,0,2, //LAY_BAC,
  1,2,0, //LAY_BCA,
  2,0,1, //LAY_CAB,
  2,1,0, //LAY_CBA
};

static void initAYClock(uint32_t clock) {
  ledc_timer_config_t ledc_timer={};
  ledc_timer.speed_mode=LEDC_HIGH_SPEED_MODE;
  ledc_timer.timer_num=LEDC_TIMER_0;
  ledc_timer.duty_resolution=LEDC_TIMER_1_BIT;
  ledc_timer.freq_hz=clock;
  ledc_timer.clk_cfg=LEDC_USE_APB_CLK; 
  ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
  ledc_channel_config_t ledc_channel={};
  ledc_channel.speed_mode=LEDC_HIGH_SPEED_MODE;
  ledc_channel.channel=LEDC_CHANNEL_0;
  ledc_channel.timer_sel=LEDC_TIMER_0;
  ledc_channel.intr_type=LEDC_INTR_DISABLE;
  ledc_channel.gpio_num=AY_CLK;
  ledc_channel.duty=1; 
  ledc_channel.hpoint=0;
  ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void out_595_byte(byte x){
  byte ii=0b10000000;
  for(int i=0;i<8;i++){
    if(ii&x)
      GPIO.out_w1ts=(1<<OUT_SHIFT_DATA_PIN);
    else
      GPIO.out_w1tc=(1<<OUT_SHIFT_DATA_PIN);
    ii>>=1;
    GPIO.out_w1ts=(1<<OUT_SHIFT_CLOCK_PIN);
    GPIO.out_w1tc=(1<<OUT_SHIFT_CLOCK_PIN);
  }
}

void out_595_word(byte lo,byte hi){
  GPIO.out_w1tc=(1<<OUT_SHIFT_LATCH_PIN);
  out_595_byte(hi);
  out_595_byte(lo);
  GPIO.out_w1ts=(1<<OUT_SHIFT_LATCH_PIN);
}

void sendAY(){
  out_595_word(outLo,outHi);
}

void ay_set_clock(uint32_t f){
  initAYClock(f);
}

void ay_write(uint8_t chip,uint8_t reg,uint8_t val){
  chip?ay_reg_1[reg]=val:ay_reg_2[reg]=val;
  bitClear(outHi, BIT_AY_BC1); // set zero to BC1, BDIR
  (!chip)?bitClear(outHi, BIT_AY_BDIR1):bitClear(outHi, BIT_AY_BDIR2);
  sendAY();
  // delayMicroseconds(5);
  outLo = (reg & B00001111); // set AY port 0...15 to D0...D3, set zero to D4...D7
  sendAY();
  bitSet(outHi, BIT_AY_BC1); // set 1 to BC1, BDIR
  (!chip)?bitSet(outHi, BIT_AY_BDIR1):bitSet(outHi, BIT_AY_BDIR2);
  sendAY();
  // delayMicroseconds(5);
  bitClear(outHi, BIT_AY_BC1); // set zero to BC1, BDIR
  (!chip)?bitClear(outHi, BIT_AY_BDIR1):bitClear(outHi, BIT_AY_BDIR2);
  sendAY();
  // delayMicroseconds(5);
  outLo = val; // set data bits to D0...D7
  sendAY();
  (!chip)?bitSet(outHi, BIT_AY_BDIR1):bitSet(outHi, BIT_AY_BDIR2); // set 1 to BDIR
  sendAY();
  // delayMicroseconds(5);
  bitClear(outHi, BIT_AY_BC1); // set zero to BC1, BDIR
  (!chip)?bitClear(outHi, BIT_AY_BDIR1):bitClear(outHi, BIT_AY_BDIR2);
  sendAY();
  // delayMicroseconds(1);
  if(PlayerCTRL.screen_mode==SCR_PLAYER) readEQ(reg,val,/*AY_EQ,*/chip);
}

void ay_write_remap(uint8_t chip,uint8_t reg,uint8_t val){
  uint8_t old;
  uint32_t off;
  if (reg>=16) return;
  if(reg==0x07){ //mixer bits remapping
    off=Config.ay_layout*3;
    old=val;
    val=old&0xc0;
    val|=((old>>0)&0x09)<<ay_mixer_remap_table[off+0];
    val|=((old>>1)&0x09)<<ay_mixer_remap_table[off+1];
    val|=((old>>2)&0x09)<<ay_mixer_remap_table[off+2];
  }else{
    reg=ay_channel_remap_table[Config.ay_layout*16+reg];
  }
  ay_write(chip,reg,val);
}

uint8_t ay_read(u_int8_t chip, uint8_t reg){
  // pseudo read
  if (reg<16){
    if(!chip) return ay_reg_1[reg]; else return ay_reg_2[reg];
  }
  return 0;
}

void ay_mute(uint8_t chip){
  ay_write(chip, 0x07, 0xff);
  ay_write(chip, 0x08, 0x00);
  ay_write(chip, 0x09, 0x00);
  ay_write(chip, 0x0a, 0x00);
}

void ay_reset(){
  bitClear(outHi,BIT_AY_BC1); // reset BC1
  bitClear(outHi,BIT_AY_BDIR1); // reset BDIR
  bitClear(outHi,BIT_AY_BDIR2); // reset BDIR
  bitClear(outHi,BIT_AY_RESET); // reset AY
  sendAY();
  delayMicroseconds(5);
  bitSet(outHi,BIT_AY_RESET); // unreset AY
  sendAY();
  delayMicroseconds(5);
  for(int i=0;i<16;i++){
    ay_write(0,i,0);
    ay_write(1,i,0);
  }
}

void AYInit(){
  ay_set_clock(Config.ay_clock);
  pinMode(OUT_SHIFT_LATCH_PIN, OUTPUT);
  pinMode(OUT_SHIFT_DATA_PIN, OUTPUT);
  pinMode(OUT_SHIFT_CLOCK_PIN, OUTPUT);
  digitalWrite(OUT_SHIFT_LATCH_PIN, HIGH);
}

void IRAM_ATTR AYTimer_ISR(){
  // if(PlayerCTRL.music_type!=TYPE_AY){
    if(PlayerCTRL.isPlay&&!PlayerCTRL.isFinish){
      if(writeFlag&&!PlayerCTRL.isFastForward) music_play();
      if(PlayerCTRL.isFastForward) music_play();
    }else{
      ay_mute(0);
      ay_mute(1);
    }
  // }
  writeFlag = !writeFlag;
}

void AYTimerInit(){
  AYTimer=timerBegin(1,80,true); // timer_id = 0; divider=80; countUp = true;
  timerAttachInterrupt(AYTimer,&AYTimer_ISR,true); // edge = true
  timerAlarmWrite(AYTimer,10*1000,true); //1000 ms
  timerAlarmEnable(AYTimer);
}
