#include <bcm2835.h>
#include "spi_RFID.h"
#include <stdio.h>

void RFID_init(){

  //SPI
  
  bcm2835_spi_begin();
 
  bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
  bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
  bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_65536);
  bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
  bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
 
  writeMFRC522(CommandReg, PCD_RESETPHASE);

  writeMFRC522(TModeReg, 0x8D);   //Tauto=1; f(Timer) = 6.78MHz/TPreScaler
  writeMFRC522(TPrescalerReg, 0x3E);  //TModeReg[3..0] + TPrescalerReg
  writeMFRC522(TReloadRegL, 30);
  writeMFRC522(TReloadRegH, 0);
  writeMFRC522(TxAutoReg, 0x40);    //100%ASK
  writeMFRC522(ModeReg, 0x3D);    // CRC valor inicial de 0x6363

  antennaOn();    //打开天线
}

void writeMFRC522(unsigned char Address, unsigned char value){
    char buff[2];

    buff[0] = (char)((Address<<1)&0x7E);
    buff[1] = (char)value;
    
    bcm2835_spi_transfern(buff,2); 
}


unsigned char readMFRC522(unsigned char Address){
    char buff[2];
    buff[0] = ((Address<<1)&0x7E)|0x80;
    bcm2835_spi_transfern(buff,2);
    return (uint8_t)buff[1];
}

void setBitMask(unsigned char reg, unsigned char mask){
  unsigned char tmp;
  tmp = readMFRC522(reg);
  writeMFRC522(reg, tmp | mask);  // set bit mask
}


void clearBitMask(unsigned char reg, unsigned char mask){
  unsigned char tmp;
  tmp = readMFRC522(reg);
  writeMFRC522(reg, tmp & (~mask));  // clear bit mask
}


void antennaOn(void){
  unsigned char temp;

  temp = readMFRC522(TxControlReg);
  if (!(temp & 0x03)){
    setBitMask(TxControlReg, 0x03);
  }
}

void antennaOff(void){
  unsigned char temp;

  temp = readMFRC522(TxControlReg);
  if (!(temp & 0x03)){
    clearBitMask(TxControlReg, 0x03);
  }
}

void calculateCRC(unsigned char *pIndata, unsigned char len, unsigned char *pOutData){
  unsigned char i, n;

  clearBitMask(DivIrqReg, 0x04);      //CRCIrq = 0
  setBitMask(FIFOLevelReg, 0x80);     //清FIFO指针
  //Write_MFRC522(CommandReg, PCD_IDLE);

  //向FIFO中写入数据
  for (i=0; i<sizeof(pIndata); i++)
    writeMFRC522(FIFODataReg, *(pIndata+i));
  writeMFRC522(CommandReg, PCD_CALCCRC);

  //等待CRC计算完成
  i = 0xFF;
  do{
    n = readMFRC522(DivIrqReg);
    i--;
  }
  while ((i!=0) && !(n&0x04));      //CRCIrq = 1

  //读取CRC计算结果
  pOutData[0] = readMFRC522(CRCResultRegL);
  pOutData[1] = readMFRC522(CRCResultRegM);
}

unsigned char MFRC522ToCard(unsigned char command, unsigned char *sendData, 
  unsigned char sendLen, unsigned char *backData, unsigned int *backLen){
  
  unsigned char status = MI_ERR;
  unsigned char irqEn = 0x00;
  unsigned char waitIRq = 0x00;
  unsigned char lastBits;
  unsigned char n;
  unsigned int i;

  switch (command){
    case PCD_AUTHENT:   //认证卡密
    {
      irqEn = 0x12;
      waitIRq = 0x10;
      break;
    }
    case PCD_TRANSCEIVE:  //发送FIFO中数据
    {
      irqEn = 0x77;
      waitIRq = 0x30;
      break;
    }
    default:
      break;
  }

  writeMFRC522(CommIEnReg, irqEn|0x80); //允许中断请求
  clearBitMask(CommIrqReg, 0x80);       //清除所有中断请求位
  setBitMask(FIFOLevelReg, 0x80);       //FlushBuffer=1, FIFO初始化

  writeMFRC522(CommandReg, PCD_IDLE);   //无动作，取消当前命令

  //向FIFO中写入数据
  for (i=0; i<sizeof(sendData); i++)
    writeMFRC522(FIFODataReg, sendData[i]);

  //执行命令
  writeMFRC522(CommandReg, command);
  if (command == PCD_TRANSCEIVE)
    setBitMask(BitFramingReg, 0x80);    //StartSend=1,transmission of data starts

  //等待接收数据完成
  i = 2000; //i根据时钟频率调整，操作M1卡最大等待时间25ms
  do{
    //CommIrqReg[7..0]
    //Set1 TxIRq RxIRq IdleIRq HiAlerIRq LoAlertIRq ErrIRq TimerIRq
    n = readMFRC522(CommIrqReg);
    i--;
  }
  while ((i!=0) && !(n&0x01) && !(n&waitIRq));

  clearBitMask(BitFramingReg, 0x80);      //StartSend=0

  if (i != 0){
    if(!(readMFRC522(ErrorReg) & 0x1B)) //BufferOvfl Collerr CRCErr ProtecolErr
    {
      status = MI_OK;
      if (n & irqEn & 0x01)
        status = MI_NOTAGERR;     //??

      if (command == PCD_TRANSCEIVE){
        n = readMFRC522(FIFOLevelReg);
        lastBits = readMFRC522(ControlReg) & 0x07;
        if (lastBits)
          *backLen = (n-1)*8 + lastBits;
        else
          *backLen = n*8;

        if (n == 0)
          n = 1;
        if (n > MAX_LEN)
          n = MAX_LEN;

        //读取FIFO中接收到的数据
        for (i=0; i
          backData[i] = readMFRC522(FIFODataReg);
      }
    }
    else
      status = MI_ERR;
  }

  //SetBitMask(ControlReg,0x80);           //timer stops
  //Write_MFRC522(CommandReg, PCD_IDLE);

  return status;
}



unsigned char findCard(unsigned char reqMode, unsigned char *TagType){
  unsigned char status;
  unsigned int backBits;      //接收到的数据位数

  writeMFRC522(BitFramingReg, 0x07);    //TxLastBists = BitFramingReg[2..0] ???

  TagType[0] = reqMode;
  status = MFRC522ToCard(PCD_TRANSCEIVE, TagType, 1, TagType, &backBits);

  if ((status != MI_OK) || (backBits != 0x10))
    status = MI_ERR;

  return status;
}


unsigned char anticoll(unsigned char *serNum){
  unsigned char status;
  unsigned char i;
  unsigned char serNumCheck=0;
  unsigned int unLen;

  clearBitMask(Status2Reg, 0x08);   //TempSensclear
  clearBitMask(CollReg,0x80);     //ValuesAfterColl
  writeMFRC522(BitFramingReg, 0x00);    //TxLastBists = BitFramingReg[2..0]

  serNum[0] = PICC_ANTICOLL;
  serNum[1] = 0x20;

  status = MFRC522ToCard(PCD_TRANSCEIVE, serNum, 2, serNum, &unLen);

  if (status == MI_OK){
    //校验卡序列号
    for (i=0; i<4; i++){
      *(serNum+i)  = serNum[i];
      serNumCheck ^= serNum[i];
    }
    if (serNumCheck != serNum[i]){
      status = MI_ERR;
    }
  }

  setBitMask(CollReg, 0x80);    //ValuesAfterColl=1

  return status;
}

 


void RFID_halt(){
  unsigned char status;
  unsigned int unLen;
  unsigned char buff[4];

  buff[0] = PICC_HALT;
  buff[1] = 0;
  calculateCRC(buff, 2, &buff[2]);

  status = MFRC522ToCard(PCD_TRANSCEIVE, buff, 4, buff,&unLen);
}

 

int main(){
  unsigned char s;
  unsigned char id[10];
  int i;
  if (!bcm2835_init()) return -1;
  RFID_init();
 
  while(1){
    if (findCard(0x52,&s)==MI_OK){
       if ( anticoll(id)==MI_OK){
         for(i=0;i<5;i++)  printf("%d ",id[i]);
         printf("\n");
       } else printf("ERR\n");
    }
    sleep(1);
  }
 
  bcm2835_spi_end();
  bcm2835_close();
  return 0;
}

