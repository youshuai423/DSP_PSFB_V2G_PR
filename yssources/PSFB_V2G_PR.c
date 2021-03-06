/******************************************************************************
| includes
|----------------------------------------------------------------------------*/
#include "PSFB_V2G_PR.h"

/******************************************************************************
| local variable definitions
|----------------------------------------------------------------------------*/
double mSample[5];
double Dutycycle = 0;
Uint16 D_Leg1 = 0;  // Epwm4
Uint16 D_Leg2 = 0;  // Epwm5
Uint16 D_Leg3 = 0;  // Epwm6
Uint16 D_LegN = 0;  // Epwm3

Uint16 LegCount = 0;  // 判断中断时控制的输出桥臂
int code_start = 0;
int indexDA = 0;
int variables_flag = 0;  // 0——控制启动， 1——DA选择
int received_data = 0;  // RXFIFO数据数

/******************************************************************************
@brief  Main
******************************************************************************/
void main()
{
   InitSysCtrl();

   DINT;

   InitPieCtrl();

   IER = 0x0000;
   IFR = 0x0000;

   InitPieVectTable();

   EALLOW;
   PieVectTable.EPWM1_INT = &epwm1_timer_isr;  // ePWM1中断入口
   PieVectTable.TINT0 = &ISRTimer0;
   EDIS;

   InitPORT();
   InitPWM();
   InitECAP();
   InitADC();
   InitSCIB();
   InitCpuTimers();  // 计算转速和转速给定值

   ConfigCpuTimer(&CpuTimer0, 150, 5000);  // 5ms
   CpuTimer0Regs.TCR.all = 0x4001; // Use write-only instruction to set TSS bit = 0

   IER |= M_INT3;  // enable ePWM CPU_interrupt
   IER |= M_INT1;  // CpuTimer

   PieCtrlRegs.PIEIER1.bit.INTx7 = 1;
   PieCtrlRegs.PIEIER3.bit.INTx1 = 1;  // enable ePWM1 pie_interrupt

   EINT;   // 总中断 INTM 使能
   ERTM;   // Enable Global realtime interrupt DBGM


   int i;
   for(; ;)
   {
	   asm("NOP");
	   for(i=1;i<=10;i++)
	   {}
   }

}

interrupt void epwm1_timer_isr(void)
{
	// Clear INT flag for this timer
	EPwm1Regs.ETCLR.bit.INT = 1;

	/* ======== 电压电流采样 ======== */
	ParallelRD(mSample, 5);
	Ia = LPfilter(mSample[0] * HallRatioIa, Ia, wc, DAB_Ts);
	Ug = LPfilter(mSample[1] * HallRatioVg, Ug, 5000, DAB_Ts);  // 采样方向和正方向相同
	Ib = LPfilter(mSample[2] * HallRatioIb, Ib, wc, DAB_Ts);
	Ig = LPfilter(mSample[3] * HallRatioIg, Ig, wc, DAB_Ts);
	Ic = Ig - Ib - Ia;  // ??合不合适

	IabcRefRatio = IgRefRatio * 0.3333;

    switch(LegCount)
    {
		case 0:  // Leg1控制
		{
			Ia_cmd = Ug * IabcRefRatio;  // 电流给定
			Ia_en = Ia_cmd - Ia;  // 电流误差

			// 输出电压给定
			Ua_cmd = PRmodule(Ag, Bg, Cg, 3 * Kr_Ig, 3 * Kp_Ig, Ia_en, &Ia_en1, &Ia_en2, \
					          &Ia_Rn1, &Ia_Rn2, Uplim_Ig, Downlim_Ig, Fleg_Ts);
			Dutycycle = 0.5 * Ua_cmd + 0.5;

			EPwm4Regs.TBPHS.half.TBPHS = Flegperiod - 400;  // 300 = 0.5*PhaSft
			EPwm5Regs.TBPHS.half.TBPHS = DABperiod * 3 - 400;
			EPwm6Regs.TBPHS.half.TBPHS = DABperiod * 2 - 400;

			EPwm3Regs.TBPHS.half.TBPHS = DABperiod - 400;

		    D_Leg1 = Flegperiod * Dutycycle;  // 第四桥臂占空比
		    EPwm4Regs.CMPA.half.CMPA = D_Leg1;  // 寄存器设置

		    LegCount++;
			break;
		}
		case 1:  // Leg2
		{
			Ib_cmd = Ug * IabcRefRatio;  // 电流给定
			Ib_en = Ib_cmd - Ib;  // 电流误差

			// 输出电压给定
			Ub_cmd = PRmodule(Ag, Bg, Cg, Kr_Ig, Kp_Ig, Ib_en, &Ib_en1, &Ib_en2, \
					          &Ib_Rn1, &Ib_Rn2, Uplim_Ig, Downlim_Ig, Fleg_Ts);
			Dutycycle = 0.5 * Ub_cmd + 0.5;

			EPwm4Regs.TBPHS.half.TBPHS = DABperiod - 400;
			EPwm5Regs.TBPHS.half.TBPHS = Flegperiod - 400;
			EPwm6Regs.TBPHS.half.TBPHS = DABperiod * 3 - 400;

			EPwm3Regs.TBPHS.half.TBPHS = DABperiod * 2 - 400;

		    D_Leg2 = Flegperiod * Dutycycle;  // B相占空比
		    EPwm5Regs.CMPA.half.CMPA = D_Leg2;  // 寄存器设置

		    LegCount++;
			break;
		}
		case 2:  // Leg3
		{
			Ic_cmd = Ug * IabcRefRatio;  // 电流给定
			Ic_en = Ic_cmd - Ic;  // 电流误差

			// 输出电压给定
			Uc_cmd = PRmodule(Ag, Bg, Cg, 3 * Kr_Ig, 3 * Kp_Ig, Ic_en, &Ic_en1, &Ic_en2, \
					          &Ic_Rn1, &Ic_Rn2, Uplim_Ig, Downlim_Ig, Fleg_Ts);
			Dutycycle = 0.5 * Uc_cmd + 0.5;

			EPwm4Regs.TBPHS.half.TBPHS = DABperiod * 2 - 400;
			EPwm5Regs.TBPHS.half.TBPHS = DABperiod - 400;
			EPwm6Regs.TBPHS.half.TBPHS = Flegperiod - 400;

			EPwm3Regs.TBPHS.half.TBPHS = DABperiod * 3 - 400;

		    D_Leg3 = Flegperiod * Dutycycle;  // B相占空比
		    EPwm6Regs.CMPA.half.CMPA = D_Leg3;  // 寄存器设置

		    LegCount++;
			break;
		}
		case 3:  // LegN
		{
			Ig_cmd = Ug * IgRefRatio;  // 电流给定
			Ig_en = Ig_cmd - Ig;  // 电流误差

			// 输出电压给定
			Ug_cmd = PRmodule(Ag, Bg, Cg, Kr_Ig, Kp_Ig, Ig_en, &Ig_en1, &Ig_en2, \
					          &Ig_Rn1, &Ig_Rn2, Uplim_Ig, Downlim_Ig, Fleg_Ts);
			Dutycycle = 0.5 * Ug_cmd + 0.5;

			EPwm4Regs.TBPHS.half.TBPHS = DABperiod * 3 - 400;
			EPwm5Regs.TBPHS.half.TBPHS = DABperiod * 2 - 400;
			EPwm6Regs.TBPHS.half.TBPHS = DABperiod - 400;

			EPwm3Regs.TBPHS.half.TBPHS = Flegperiod - 400;

		    D_LegN = Flegperiod * Dutycycle;  // B相占空比
		    EPwm3Regs.CMPA.half.CMPA = D_LegN;  // 寄存器设置

		    LegCount = 0;
			break;
		}
		default:
		{
			EPwm1Regs.TBCTL.bit.CTRMODE = TB_FREEZE;  // 向上计数
			EPwm2Regs.TBCTL.bit.CTRMODE = TB_FREEZE;
			EPwm3Regs.TBCTL.bit.CTRMODE = TB_FREEZE;
			EPwm4Regs.TBCTL.bit.CTRMODE = TB_FREEZE;
			EPwm5Regs.TBCTL.bit.CTRMODE = TB_FREEZE;
			EPwm6Regs.TBCTL.bit.CTRMODE = TB_FREEZE;
		}
    }

	switch(indexDA)
	{
		case 0:{DACout(0, Ig); DACout(2, Ug * 0.1); break;}
		case 1:{DACout(0, Ia); DACout(1, Ia_cmd); DACout(2, Ua_cmd); break;}
		case 2:{DACout(0, Ib); DACout(1, Ib_cmd); DACout(2, Ub_cmd); break;}
		case 3:{DACout(0, Ic); DACout(1, Ic_cmd); DACout(2, Uc_cmd); break;}
		default:{DACout(0, Ug * 0.1); DACout(1, 0);}
	}

    // Clear INT flag for this timer
   	while(EPwm1Regs.ETFLG.bit.INT == 1)
   		EPwm1Regs.ETCLR.bit.INT = 1;

   // Acknowledge this interrupt to receive more interrupts from group 3
    PieCtrlRegs.PIEACK.all = PIEACK_GROUP3;
}

interrupt void ISRTimer0(void)
{
	int temp = 0;
	CpuTimer0.InterruptCount ++;
	if (CpuTimer0.InterruptCount  > 15) CpuTimer0.InterruptCount -= 16;

	if(ScibRegs.SCIFFRX.bit.RXFFST == 0)
		received_data = 0;
	else if(ScibRegs.SCIFFRX.bit.RXFFST == 2)
	{
		// 接受控制对象
		scib_rx(&temp);
		switch(temp)
		{
		case 0xff: variables_flag = 0; break;
		case 0xfe: variables_flag = 1; break;
		}
		// 接受控制参数
		scib_rx(&temp);
		switch(variables_flag)
		{
		case 0: code_start = temp; break;
		case 1: indexDA = temp; break;
		}

		//scib_tx(1);
	}
	else if(received_data == 1 && ScibRegs.SCIFFRX.bit.RXFFST == 1)
	{
		received_data = 0;
		ScibRegs.SCIFFRX.bit.RXFIFORESET = 0;  // reset RXFIFO
		ScibRegs.SCIFFRX.bit.RXFIFORESET = 1;  // reenable RXFIFO
	}
	else if(ScibRegs.SCIFFRX.bit.RXFFST == 1)
		received_data = 1;
	else
		received_data = 0;

	// Acknowledge this interrupt to receive more interrupts from group 1
	PieCtrlRegs.PIEACK.all = PIEACK_GROUP1;
	CpuTimer0Regs.TCR.bit.TIF=1;
	CpuTimer0Regs.TCR.bit.TRB=1;
}
