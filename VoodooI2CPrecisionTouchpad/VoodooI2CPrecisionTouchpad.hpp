//
//  VoodooI2CPrecisionTouchpad.hpp
//  VoodooI2CPrecisionTouchpad
//
//  Created by CoolStar on 8/24/17.
//  Copyright © 2017 CoolStar. All rights reserved.
//

#ifndef VoodooI2CPrecisionTouchpad_hpp
#define VoodooI2CPrecisionTouchpad_hpp

#include <IOKit/IOService.h>
#include <IOKit/acpi/IOACPIPlatformDevice.h>
#include <IOKit/IOTimerEventSource.h>
#include "VoodooI2CControllerDriver.hpp"
#include "csgesture.h"

struct __attribute__((__packed__)) i2c_hid_descr {
    UInt16 wHIDDescLength;
    UInt16 bcdVersion;
    UInt16 wReportDescLength;
    UInt16 wReportDescRegister;
    UInt16 wInputRegister;
    UInt16 wMaxInputLength;
    UInt16 wOutputRegister;
    UInt16 wMaxOutputLength;
    UInt16 wCommandRegister;
    UInt16 wDataRegister;
    UInt16 wVendorID;
    UInt16 wProductID;
    UInt16 wVersionID;
    UInt32 reserved;
};

class VoodooI2CHIDDeviceWrapper;
class VoodooI2CPrecisionTouchpad : public IOService
{
    OSDeclareDefaultStructors(VoodooI2CPrecisionTouchpad);
private:
    VoodooI2CControllerDriver *i2cController;
    IOService *provider;
    IOInterruptEventSource *interruptSource;
    IOTimerEventSource *timerSource;
    
    CSGesture* wrapper;
    
    uint32_t hw_res_x, hw_res_y;
    
    struct csgesture_softc softc;
    
    IOWorkLoop *workLoop;
    
    UInt16 i2cAddress;
    bool use10BitAddressing;
    UInt16 HIDDescriptorAddress;
    
    bool DeviceIsAwake;
    bool IsReading;
    
    IOReturn getDescriptorAddress(IOACPIPlatformDevice *acpiDevice);
    
    IOReturn readI2C(UInt8 *values, UInt16 len);
    IOReturn writeI2C(UInt8 *values, UInt16 len);
    IOReturn writeReadI2C(UInt8 *writeBuf, UInt16 writeLen, UInt8 *readBuf, UInt16 readLen);
    
    IOReturn fetchHIDDescriptor();
    
    IOReturn set_power(int power_state);
    IOReturn reset_dev();
    
    IOReturn write_feature(uint8_t reportID, uint8_t *buf, size_t buf_len);
    
    IOReturn enable_abs();
    
public:
    struct i2c_hid_descr HIDDescriptor;
    
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
    virtual IOReturn setPowerState(unsigned long powerState, IOService *whatDevice) override;
    
    void get_input(OSObject *owner, IOTimerEventSource* sender);
    void read_input();
    
    void InterruptOccured(OSObject* owner, IOInterruptEventSource* src, int intCount);
};

#endif /* VoodooI2CHIDDevice_hpp */
