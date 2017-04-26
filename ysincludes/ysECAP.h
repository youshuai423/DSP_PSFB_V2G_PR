/******************************************************************************
| includes
|--------------------------------------------------------------------------------------------*/
#include "DSP2833x_Device.h"     // DSP2833x Headerfile Include File
#include "DSP2833x_Examples.h"   // DSP2833x Examples Include File
#include "ysPWM.h"

/******************************************************************************
| defines
|--------------------------------------------------------------------------------------------*/
#define NoneOverlap			150   // ���ص�ʱ��1us

/******************************************************************************
| variables
|--------------------------------------------------------------------------------------------*/
extern Uint16 PhaSft;

/******************************************************************************
| local functions prototypes
|--------------------------------------------------------------------------------------------*/
void InitECAP(void);