/*
 * GccApplication69.cpp
 *
 * Created: 2017/11/19 20:40:59
 * Author : Vulcan
 * kp:249 kd:68 p1:151
 */ 

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdlib.h>
#define  RX_begin_byte 0xFf
#define data_Index_MAX 1000

/*蓝牙——数据接收*/
void USART0_init();//用来接蓝牙
void USART0_send(int);//给遥控器发送确认型号 0x55
uint8_t RX_group[6];
bool RX_flag;//是否接收到手柄发过来的数组
int8_t RX_Index=0;
bool RX_COMP_flag=0;

/*串口显示器——数据发送*/
void USART1_init();//用来接串口显示器
void USART1_send(int);//给串口显示器发送数据

/*定时器——采样光电对管&PWM输出定时器*/
void TC0_init();//电机驱动定时器
void TC1_init();//红外管阵列采样定时器
int correct_data_index(int);
int8_t x1,x2,X_count,X_count1;
int8_t X=0;
int8_t data[data_Index_MAX][3]={{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0}};//数据记录数组 {时间高位，时间低位，位置}
int8_t v[1000];//记录时间 为了节约程序储存器空间我把分开放置
uint8_t kp,ki;
uint8_t OCR0A_base = 128,OCR0B_base = 128;
int _OCR0A=0,_OCR0B=0;
int data_Index=7,time_Index=0;
int v_temp=0;
int8_t x_error=0;
/*EEPROM——按下按钮数据保存到EEPROM*/
void INT0_init();
int8_t v1=0,v2=0;
uint8_t EEPROM_write_flag=0;
void old_data_load();

/*加速——直线加速功能*/
uint8_t fast_mode_count=0;
uint8_t fast_mode_enable=0;

/*模式——模式选择功能*/
uint8_t Mode_select_raw=0;
uint8_t Mode_select=0;
int stop_state=0;	//光电对管检测到纯黑区域的次数的储存器
uint8_t stop_control0=0;//常规组赛道停车控制位
uint8_t stop_control1=0;//创新组赛道停车控制位
uint8_t stop_area_num=0;//经过停车区域的次数
uint8_t black_state=0;	//创新组2号赛道
uint8_t stop_area_enable=1;
uint8_t mode2_black_area_enable=0;
uint8_t mode2_black_line_num=0;
uint8_t ring_flag=0;
uint8_t mode3_right_flag=0;
uint8_t mode3_left_flag=0;

/*延时停车计数变量*/
//1000次记一下 一共50s 计时两百下
int time_data=0;
uint8_t time_data_enable=0;

int main(void)
{
  SREG|=0x80;
  DDRG|=0x03;
  //PORTG=0x03;
  INT0_init();
  USART1_init();
  USART0_init();
  USART1_send(0x00);
  TC0_init ();
  TC1_init ();
  USART1_send(0x01);
  
  /*接光电对管*/
  PORTA |=0XFF;
  PORTC |=0XFF;
  
    /*接蜂鸣器*/
  DDRL=(1<<DDL3)|(1<<DDL5)|(1<<DDL7);
  PORTL =(1<<DDL7)|(1<<DDL5);
  /*从EEPROM里面把过去的数据读出来*/
  old_data_load();
  
  /*模式选择——PORTF端口全部设置为拉低时输出低电平*/
  DDRF=0x00;
  //DDRK=0XFF;
  PORTF|=0xff;
  //PORTK=0xff;
  /*等待首次完成数据接收*/
    while (1) 
    {
	/*模式选择——采集，处理*/
	  Mode_select_raw=~PINF;
	  for (int i=0;i<8;i++)
	  {
		  if ((Mode_select_raw&(1<<i))==(1<<i)){Mode_select=i;PORTK=(1<<i);}
	  }

    if (RX_COMP_flag==1)
    {
      //把从遥控端发来的数据搬运到内部的储存器中
      kp = (RX_group[0]<<7)|(RX_group[1]);
      ki = (RX_group[2]<<7)|(RX_group[3]);
      OCR0A_base=RX_group[4];
      OCR0B_base=RX_group[5];
      RX_COMP_flag=0;
    }
    //把这个数据发送给串口显示器 kp ki x
    USART1_send(RX_begin_byte);
    USART1_send(RX_group[0]); //byte0
    USART1_send(RX_group[1]);//byte1
    USART1_send(RX_group[2]);//byte2
    USART1_send(RX_group[3]);//byte3
    USART1_send(OCR0A_base);//byte4
    USART1_send(OCR0B_base);//byte5
    //USART1_send(x1);
    USART1_send(data[correct_data_index(data_Index-1)][2]);//byte6
    //USART1_send(x2);//byte7
    USART1_send(v[correct_data_index(data_Index-1)]);//byte7
    USART1_send(time_data_enable);//byte8
    //USART1_send(X_count1);//byte8
    USART1_send(time_data);//byte9
    
    //给遥控器发送一个确认信号
    USART0_send(RX_begin_byte);
    USART0_send(data[correct_data_index(data_Index-1)][2]);
    USART0_send(_OCR0A);
    USART0_send(_OCR0B);
    
    //中断触发之后开始记录数据
    while (EEPROM_write_flag)
    {
      for(int i=0;i<=6;i++)
      {
        while(EECR & (1<<EEPE));
        /* Set up address and Data Registers */
        EEAR = i;
        EEDR = RX_group[i];
        /* Write logical one to EEMPE */
        EECR |= (1<<EEMPE);
        /* Start eeprom write by setting EEPE */
        EECR |= (1<<EEPE);
      }
      EEPROM_write_flag=0;
    }
    }
}

void USART0_init()
{
  UCSR0B |=(1<<RXEN0)|(1<<TXEN0)|(1<<RXCIE0);
  UCSR0C |=(1<<UCSZ01)|(1<<UCSZ00);
  UBRR0L =103;
}

void USART1_init()
{
  UCSR1B |=(1<<RXEN1)|(1<<TXEN1);
  UCSR1C |=(1<<UCSZ11)|(1<<UCSZ10);
  UBRR1L =103;
}
void USART0_send(int data)
{
  while ( !( UCSR0A & (1<<UDRE0)) );
  UDR0 = data;
}

void USART1_send(int data)
{
  while ( !( UCSR1A & (1<<UDRE1)));
  UDR1 = data;
}

void TC0_init ()
{
  TCCR0A |=(1<<WGM01)|(1<<WGM00)|(1<<COM0A1);
  TCCR0B |=(1<<CS02)|(1<<CS00);
  OCR0A=OCR0A_base;

  TCCR0A |=(1<<COM0B1);
  OCR0B=OCR0B_base;
  
  DDRB |=(1<<DDB7);
  PORTB|=(1<<PORTB7);
  DDRG |=(1<<DDG5);
  PORTG|=(1<<DDG5);  
}

void INT0_init()
{
  PORTD |=0x01;
  _delay_ms(5);
  EICRA |=(1<<ISC01);
  EIMSK |=(1<<INT0);
}
/*采样定时器 一共5s*/
void TC1_init ()
{
  TCCR1B |= (1<<WGM12);//定义为CTC模式
  TCCR1B |=(1<<CS11);//8分频 CPU频率16Mhz 单次时间0.5us
  OCR1A=500; //计数TOP值 0.5us*1000=0.5ms
  TIMSK1 |=(1<<OCIE1A);//输出比较A匹配中断使能
}
/*只用来给出data_index的应该指代的数字,不改变data_index的大小*/
int correct_data_index(int n)
{
  if (n>=data_Index_MAX)
  {
    n = n-data_Index_MAX;
  }
  else if (n<0)
  {
    n +=data_Index_MAX;
  }
  return n;
}

void old_data_load()
{
  for(int i=0;i<=6;i++)
  {
    while(EECR & (1<<EEPE));
    EEAR = i;
    EECR |= (1<<EERE);   
    RX_group[i]=EEDR;
  }
  RX_COMP_flag=1;
}
//采样中断服务
ISR (TIMER1_COMPA_vect)
{
	 /*数组格式 
	  ——数组data  {记录时间（高8位），记录时间（低8位），黑线位置}
	  左右摇摆速度有单独的数组记录 
	  ——数组V 浮点型
	  */	
  
  /*采样*/
  x1 = PINA;
  x2 = PINC;
  
  //黑线位置——分析光电对管的数据 得出x在哪 有几个被激发了
  for (int i=7;i>=0;i--)
  {
    if ((x1&(1<<i))==(1<<i)){X=i;X_count++;}
    if ((x2&(1<<i))==(1<<i)){X=-i-1;X_count++;}
  }

  /*数据处理*/
  //错误检测——如果两个以下的光电对管检测到黑线*/
  if ((X_count>0)&&(X_count<=2))
  {
    //记录位置
    data[data_Index][2]=X;
	//计算速度——0.5ms*4内的平均速度
	for (int m=0;m<4;m++)
	{
	     v_temp += data[correct_data_index(data_Index-m)][2]-data[correct_data_index(data_Index-10-m)][2];
	}
    x_error=X-data[correct_data_index(data_Index-1)][2];
    if (abs(x_error)>=8)
    {
   	//记录位置超出赛道后
	   data[data_Index][2]=data[correct_data_index(data_Index-2)][2];
	   v[data_Index]=v[correct_data_index(data_Index-2)];
    }
    else
    {
	    data[data_Index][2]=X;
	    v[data_Index]=v_temp;
    }
	stop_state=0;
	stop_area_enable=0;
	mode2_black_area_enable=0;
  }
  //激发个数超过15个 可能是进入停车区 若在模式2可能是要拐弯了
//   else if (X_count>=15)
//   {	
// 		data[data_Index][2]=data[correct_data_index(data_Index-2)][2];
// 		v[data_Index]=v[correct_data_index(data_Index-2)];
//   }
  else //否则就忽略这组数据，让这次的数据维持上一组数据的值
  {
    data[data_Index][2]=data[correct_data_index(data_Index-1)][2];
    v[data_Index]=v[correct_data_index(data_Index-1)];
	stop_state=0;
	stop_area_enable=0;
	mode2_black_area_enable=0;
  }
    
/*PID部分*/  
  if ((X<=1)&&(X>=-1))
  {
    if (data_Index%100==0)
    {
    fast_mode_count++;
    }
    if (fast_mode_count>=8)//800*0.5ms=0.4s
    {
    _OCR0A=200+kp*(data[data_Index][2])/10+ki*(v[data_Index])/100;
    _OCR0B=200-kp*(data[data_Index][2])/10-ki*(v[data_Index])/100;
    }
   else
   {
   _OCR0A = OCR0A_base+kp*(data[data_Index][2])/8+ki*(v[data_Index])/100;
   _OCR0B = OCR0B_base-kp*(data[data_Index][2])/8-ki*(v[data_Index])/100;     
	}
  }
  else if ((X>=6)||(X<=-7))
  {
    _OCR0A = OCR0A_base+kp*(data[data_Index][2])/5;
    _OCR0B = OCR0B_base-kp*(data[data_Index][2])/5;
    fast_mode_count=0;
  }
  else
  {
    _OCR0A = OCR0A_base+kp*(data[data_Index][2])/8+ki*(v[data_Index])/100;
    _OCR0B = OCR0B_base-kp*(data[data_Index][2])/8-ki*(v[data_Index])/100;
    fast_mode_count=0;
  }
  
  _OCR0A+=10;
  _OCR0B+=10;
/*计数值防溢出*/
  data_Index++;
  data_Index=correct_data_index(data_Index);
  
/*速度防溢出*/
  if (_OCR0A>=255){_OCR0A=255;}
  if (_OCR0B>=255){_OCR0B=255;}
  PORTG&=~0x03;//清零低两位
  if (_OCR0A<0){_OCR0A=255+_OCR0A;PORTG|=(1<<DDG0);}
  if (_OCR0B<0){_OCR0B=255+_OCR0B;PORTG|=(1<<DDG1);}
	  
/*定时停车*/	
	//过黑线触发
	if (X_count>=10)
	{
		time_data_enable=1;
	}
	//触发后计时 1000*0.25ms =0.25s
	if (time_data_enable==1)
	{
		if (data_Index==999)
		{
			time_data++;
		}
	}
	
	OCR0A=_OCR0A;
	OCR0B=_OCR0B;
	
	v_temp=0;
	X_count=0;//计数器——受触发的光电对管 清零
}

ISR (USART0_RX_vect)
{
  int UDR_TEMP = UDR0;
  if ((RX_flag == 1)&&(RX_Index>=0))
  {
    RX_group[RX_Index]=UDR_TEMP;
    RX_Index++;
  }
  if(RX_Index>=6)
  {
    RX_Index = 0;//数组指针置零
    RX_flag = 0;//起始帧标志位清零
    RX_COMP_flag=1;//4个8bit接收完成
  }
  if (UDR_TEMP==RX_begin_byte){RX_flag = 1;}//接收到起始帧
}
ISR (INT0_vect)
{
  EEPROM_write_flag=1;
}