configuration FlashDriverC
{
    provides interface Driver;
    uses interface FlashAttr;
}
implementation
{
    components RealMainP;
    components FlashDriverP;


    components HplSam4lIOC, new Sam4lSPI0C();

    FlashDriverP.Resource -> Sam4lSPI0C;
    FlashDriverP.CS -> HplSam4lIOC.PC03;
    FlashDriverP.FastSpiByte -> Sam4lSPI0C;
    FlashDriverP.HplSam4lSPIChannel -> Sam4lSPI0C;
    FlashDriverP.Init <- RealMainP.SoftwareInit;
    FlashDriverP.FlashAttr = FlashAttr;
    Driver = FlashDriverP.Driver;

   // components HplSam4lTWIMP;
   // components McuSleepC;
   // components HplSam4lClockC;
   // components new Sam4lDMAChannelC() as dmac1;
   // components new Sam4lDMAChannelC() as dmac2;
   // HplSam4lTWIMP.ClockCtl[1] -> HplSam4lClockC.TWIM1Ctl;
   // HplSam4lTWIMP.ClockCtl[2] -> HplSam4lClockC.TWIM2Ctl;
   // HplSam4lTWIMP.dmac[1] -> dmac1;
   // HplSam4lTWIMP.dmac[2] -> dmac2;
   // HplSam4lTWIMP.IRQWrapper -> McuSleepC;
   //
   // I2CDriverP.HplSam4lTWIM -> HplSam4lTWIMP.TWIM;
//
   // Driver = I2CDriverP.Driver;
}
