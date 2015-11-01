module BernardP
{
    uses
    {
        interface SplitControl as IPControl;
        interface RootControl;
    }
}
implementation
{
    task void init() {
        //call RootControl.setRoot();
        //call RawSocket.initialize(41);
        call RootControl.setRoot();
    }

    event void IPControl.startDone (error_t error) {
        post init();
    }

    event void IPControl.stopDone (error_t error) {}
}
