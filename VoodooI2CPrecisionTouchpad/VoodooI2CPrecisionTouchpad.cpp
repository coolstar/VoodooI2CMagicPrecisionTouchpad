//
//  VoodooPrecisionTouchpad.cpp
//  VoodooI2CPrecisionTouchpad
//
//  Created by CoolStar on 8/24/17.
//  Copyright © 2017 CoolStar. All rights reserved.
//  ported from crostrackpad-elan 3.0 for Windows
//  based off VoodooElanTouchpadDevice
//

/*
 * NOTE: THIS IS A TEMPLATE DEVICE. IT HAS STRUCTS AND REPORT ID'S THAT
 * NEED TO BE ADJUSTED FOR YOUR TOUCHPAD. IF YOU DO NOT HAVE AN ELAN0651
 * YOU NEED TO MODIFY THIS DRIVER BEFORE USING IT.
 */

#include "VoodooI2CPrecisionTouchpad.hpp"
#include <IOKit/IOLib.h>

#define super IOService

#define I2C_HID_PWR_ON  0x00
#define I2C_HID_PWR_SLEEP 0x01

#define INPUT_MODE_MOUSE 0x00
#define INPUT_MODE_TOUCHPAD 0x03

// Begin Touchpad Specific Structs

#define CONFIDENCE_BIT 1
#define TIPSWITCH_BIT 2

#define INPUT_CONFIG_REPORT_ID 3
#define TOUCHPAD_REPORT_ID 4

struct __attribute__((__packed__)) INPUT_MODE_FEATURE {
    uint8_t ReportID;
    uint8_t Mode;
    uint8_t Reserved;
};

struct __attribute__((__packed__)) TOUCH {
    uint8_t ContactInfo; //Contact ID (4), Status (4)
    uint16_t XValue;
    uint16_t YValue;
};

struct __attribute__((__packed__)) TOUCHPAD_INPUT_REPORT {
    uint8_t ReportID;
    TOUCH MTouch;
    uint16_t ScanTime;
    uint8_t ContactCount;
    uint8_t Button;
    uint16_t Reserved;
};

// End Touchpad Specific Structs

union command {
    UInt8 data[4];
    struct __attribute__((__packed__)) cmd {
        UInt16 reg;
        UInt8 reportTypeID;
        UInt8 opcode;
    } c;
};

struct __attribute__((__packed__))  i2c_hid_cmd {
    unsigned int registerIndex;
    UInt8 opcode;
    unsigned int length;
    bool wait;
};

OSDefineMetaClassAndStructors(VoodooI2CPrecisionTouchpad, IOService);

bool VoodooI2CPrecisionTouchpad::start(IOService *provider){
    if (!super::start(provider))
        return false;
    
    this->DeviceIsAwake = false;
    this->IsReading = true;
    
    PMinit();
    
    IOLog("%s::Starting!\n", getName());
    
    i2cController = OSDynamicCast(VoodooI2CControllerDriver, provider->getProvider());
    if (!i2cController){
        IOLog("%s::Unable to get I2C Controller!\n", getName());
        return false;
    }
    
    OSNumber *i2cAddress = OSDynamicCast(OSNumber, provider->getProperty("i2cAddress"));
    this->i2cAddress = i2cAddress->unsigned16BitValue();
    
    OSNumber *addrWidth = OSDynamicCast(OSNumber, provider->getProperty("addrWidth"));
    this->use10BitAddressing = (addrWidth->unsigned8BitValue() == 10);
    
    IOACPIPlatformDevice *acpiDevice = OSDynamicCast(IOACPIPlatformDevice, provider->getProperty("acpi-device"));
    if (getDescriptorAddress(acpiDevice) != kIOReturnSuccess){
        IOLog("%s::Unable to get HID Descriptor address!\n", getName());
        PMstop();
        return false;
    }
    
    IOLog("%s::Got HID Descriptor Address!\n", getName());
    
    if (fetchHIDDescriptor() != kIOReturnSuccess){
        IOLog("%s::Unable to get HID Descriptor!\n", getName());
        PMstop();
        return false;
    }
    
    IOLog("%s::Got HID Descriptor!\n", getName());
    
    this->IsReading = false;
    
    this->workLoop = getWorkLoop();
    if (!this->workLoop){
        IOLog("%s::Unable to get workloop\n", getName());
        stop(provider);
        return false;
    }
    
    this->workLoop->retain();
    
    this->interruptSource = IOInterruptEventSource::interruptEventSource(this, OSMemberFunctionCast(IOInterruptEventAction, this, &VoodooI2CPrecisionTouchpad::InterruptOccured), provider, 0);
    if (!this->interruptSource) {
        IOLog("%s::Unable to get interrupt source\n", getName());
        stop(provider);
        return false;
    }
    
    this->workLoop->addEventSource(this->interruptSource);
    
    this->magicMultitouchDevice = new CSMagicMultitouchDevice;
    this->magicMultitouchDevice->init();
    this->magicMultitouchDevice->attach(this);
    this->magicMultitouchDevice->start(this);
    
    registerService();
    
    reset_dev();
    enable_abs();
    
    uint16_t max_x = 3209;
    uint16_t max_y = 2097;
    
    for (int i = 0;i < MAX_FINGERS; i++) {
        sc.x[i] = -1;
        sc.y[i] = -1;
        sc.p[i] = -1;
    }
    
    this->DeviceIsAwake = true;
    this->IsReading = false;
    
    this->interruptSource->enable();
    
#define kMyNumberOfStates 2
    
    static IOPMPowerState myPowerStates[kMyNumberOfStates];
    // Zero-fill the structures.
    bzero (myPowerStates, sizeof(myPowerStates));
    // Fill in the information about your device's off state:
    myPowerStates[0].version = 1;
    myPowerStates[0].capabilityFlags = kIOPMPowerOff;
    myPowerStates[0].outputPowerCharacter = kIOPMPowerOff;
    myPowerStates[0].inputPowerRequirement = kIOPMPowerOff;
    // Fill in the information about your device's on state:
    myPowerStates[1].version = 1;
    myPowerStates[1].capabilityFlags = kIOPMPowerOn;
    myPowerStates[1].outputPowerCharacter = kIOPMPowerOn;
    myPowerStates[1].inputPowerRequirement = kIOPMPowerOn;
    
    provider->joinPMtree(this);
    
    registerPowerDriver(this, myPowerStates, kMyNumberOfStates);
    return true;
}

void VoodooI2CPrecisionTouchpad::stop(IOService *provider){
    IOLog("%s::Stopping!\n", getName());
    
    this->DeviceIsAwake = false;
    IOSleep(1);
    
    if (this->interruptSource){
        this->interruptSource->disable();
        this->workLoop->removeEventSource(this->interruptSource);
        
        this->interruptSource->release();
        this->interruptSource = NULL;
    }
    
    if (this->magicMultitouchDevice){
        this->magicMultitouchDevice->stop(this);
        this->magicMultitouchDevice->terminate(kIOServiceRequired | kIOServiceSynchronous);
        this->magicMultitouchDevice->release();
        this->magicMultitouchDevice = NULL;
    }
    
    OSSafeReleaseNULL(this->workLoop);
    
    PMstop();
    
    super::stop(provider);
}

IOReturn VoodooI2CPrecisionTouchpad::setPowerState(unsigned long powerState, IOService *whatDevice){
    if (whatDevice != this)
        return kIOReturnInvalid;
    if (powerState == 0){
        //Going to sleep
        if (this->DeviceIsAwake){
            this->DeviceIsAwake = false;
            while (this->IsReading){
                IOSleep(10);
            }
            this->IsReading = true;
            set_power(I2C_HID_PWR_SLEEP);
            this->IsReading = false;
            
            IOLog("%s::Going to Sleep!\n", getName());
        }
    } else {
        if (!this->DeviceIsAwake){
            this->IsReading = true;
            reset_dev();
            enable_abs();
            this->IsReading = false;
            
            this->DeviceIsAwake = true;
            IOLog("%s::Woke up from Sleep!\n", getName());
        } else {
            IOLog("%s::Device already awake! Not reinitializing.\n", getName());
        }
    }
    return kIOPMAckImplied;
}

IOReturn VoodooI2CPrecisionTouchpad::getDescriptorAddress(IOACPIPlatformDevice *acpiDevice){
    if (!acpiDevice)
        return kIOReturnNoDevice;
    
    UInt32 guid_1 = 0x3CDFF6F7;
    UInt32 guid_2 = 0x45554267;
    UInt32 guid_3 = 0x0AB305AD;
    UInt32 guid_4 = 0xDE38893D;
    
    OSObject *result = NULL;
    OSObject *params[4];
    char buffer[16];
    
    memcpy(buffer, &guid_1, 4);
    memcpy(buffer + 4, &guid_2, 4);
    memcpy(buffer + 8, &guid_3, 4);
    memcpy(buffer + 12, &guid_4, 4);
    
    
    params[0] = OSData::withBytes(buffer, 16);
    params[1] = OSNumber::withNumber(0x1, 8);
    params[2] = OSNumber::withNumber(0x1, 8);
    params[3] = OSNumber::withNumber((unsigned long long)0x0, 8);
    
    acpiDevice->evaluateObject("_DSM", &result, params, 4);
    if (!result)
        acpiDevice->evaluateObject("XDSM", &result, params, 4);
    if (!result)
        return kIOReturnNotFound;
    
    OSNumber* number = OSDynamicCast(OSNumber, result);
    if (number){
        setProperty("HIDDescriptorAddress", number);
        this->HIDDescriptorAddress = number->unsigned16BitValue();
    }
    
    if (result)
        result->release();
    
    params[0]->release();
    params[1]->release();
    params[2]->release();
    params[3]->release();
    
    if (!number)
        return kIOReturnInvalid;
    
    return kIOReturnSuccess;
}

IOReturn VoodooI2CPrecisionTouchpad::readI2C(UInt8 *values, UInt16 len){
    UInt16 flags = I2C_M_RD;
    if (this->use10BitAddressing)
        flags = I2C_M_RD | I2C_M_TEN;
    VoodooI2CControllerBusMessage msgs[] = {
        {
            .address = this->i2cAddress,
            .buffer = values,
            .flags = flags,
            .length = (UInt16)len,
        },
    };
    return i2cController->transferI2C(msgs, 1);
}

IOReturn VoodooI2CPrecisionTouchpad::writeI2C(UInt8 *values, UInt16 len){
    UInt16 flags = 0;
    if (this->use10BitAddressing)
        flags = I2C_M_TEN;
    VoodooI2CControllerBusMessage msgs[] = {
        {
            .address = this->i2cAddress,
            .buffer = values,
            .flags = flags,
            .length = (UInt16)len,
        },
    };
    return i2cController->transferI2C(msgs, 1);
}

IOReturn VoodooI2CPrecisionTouchpad::writeReadI2C(UInt8 *writeBuf, UInt16 writeLen, UInt8 *readBuf, UInt16 readLen){
    UInt16 readFlags = I2C_M_RD;
    if (this->use10BitAddressing)
        readFlags = I2C_M_RD | I2C_M_TEN;
    UInt16 writeFlags = 0;
    if (this->use10BitAddressing)
        writeFlags = I2C_M_TEN;
    VoodooI2CControllerBusMessage msgs[] = {
        {
            .address = this->i2cAddress,
            .buffer = writeBuf,
            .flags = writeFlags,
            .length = writeLen,
        },
        {
            .address = this->i2cAddress,
            .buffer = readBuf,
            .flags = readFlags,
            .length = readLen,
        }
    };
    return i2cController->transferI2C(msgs, 2);
}

IOReturn VoodooI2CPrecisionTouchpad::fetchHIDDescriptor(){
    UInt8 length = 2;
    
    union command cmd;
    cmd.c.reg = this->HIDDescriptorAddress;
    
    memset((UInt8 *)&this->HIDDescriptor, 0, sizeof(i2c_hid_descr));
    
    if (writeReadI2C(cmd.data, (UInt16)length, (UInt8 *)&this->HIDDescriptor, (UInt16)sizeof(i2c_hid_descr)) != kIOReturnSuccess)
        return kIOReturnIOError;
    
    IOLog("%s::BCD Version: 0x%x\n", getName(), this->HIDDescriptor.bcdVersion);
    if (this->HIDDescriptor.bcdVersion == 0x0100 && this->HIDDescriptor.wHIDDescLength == sizeof(i2c_hid_descr)){
        setProperty("HIDDescLength", (UInt32)this->HIDDescriptor.wHIDDescLength, 32);
        setProperty("bcdVersion", (UInt32)this->HIDDescriptor.bcdVersion, 32);
        setProperty("ReportDescLength", (UInt32)this->HIDDescriptor.wReportDescLength, 32);
        setProperty("ReportDescRegister", (UInt32)this->HIDDescriptor.wReportDescRegister, 32);
        setProperty("InputRegister", (UInt32)this->HIDDescriptor.wInputRegister, 32);
        setProperty("MaxInputLength", (UInt32)this->HIDDescriptor.wMaxInputLength, 32);
        setProperty("OutputRegister", (UInt32)this->HIDDescriptor.wOutputRegister, 32);
        setProperty("MaxOutputLength", (UInt32)this->HIDDescriptor.wMaxOutputLength, 32);
        setProperty("CommandRegister", (UInt32)this->HIDDescriptor.wCommandRegister, 32);
        setProperty("DataRegister", (UInt32)this->HIDDescriptor.wDataRegister, 32);
        setProperty("vendorID", (UInt32)this->HIDDescriptor.wVendorID, 32);
        setProperty("productID", (UInt32)this->HIDDescriptor.wProductID, 32);
        setProperty("VersionID", (UInt32)this->HIDDescriptor.wVersionID, 32);
        return kIOReturnSuccess;
    }
    return kIOReturnDeviceError;
}

IOReturn VoodooI2CPrecisionTouchpad::set_power(int power_state){
    uint8_t length = 4;
    
    union command cmd;
    cmd.c.reg = this->HIDDescriptor.wCommandRegister;
    cmd.c.opcode = 0x08;
    cmd.c.reportTypeID = power_state;
    
    return writeI2C(cmd.data, length);
}

IOReturn VoodooI2CPrecisionTouchpad::reset_dev(){
    set_power(I2C_HID_PWR_ON);
    
    IOSleep(1);
    
    uint8_t length = 4;
    
    union command cmd;
    cmd.c.reg = this->HIDDescriptor.wCommandRegister;
    cmd.c.opcode = 0x01;
    cmd.c.reportTypeID = 0;
    
    writeI2C(cmd.data, length);
    return kIOReturnSuccess;
}

IOReturn VoodooI2CPrecisionTouchpad::write_feature(uint8_t reportID, uint8_t *buf, size_t buf_len){
    uint16_t dataReg = this->HIDDescriptor.wDataRegister;
    
    uint8_t reportType = 0x03;
    
    uint8_t idx = 0;
    
    uint16_t size;
    uint16_t args_len;
    
    size = 2 +
    (reportID ? 1 : 0)	/* reportID */ +
    buf_len		/* buf */;
    args_len = (reportID >= 0x0F ? 1 : 0) /* optional third byte */ +
    2			/* dataRegister */ +
    size			/* args */;
    
    uint8_t *args = (uint8_t *)IOMalloc(args_len);
    memset(args, 0, args_len);
    
    if (reportID >= 0x0F) {
        args[idx++] = reportID;
        reportID = 0x0F;
    }
    
    args[idx++] = dataReg & 0xFF;
    args[idx++] = dataReg >> 8;
    
    args[idx++] = size & 0xFF;
    args[idx++] = size >> 8;
    
    if (reportID)
        args[idx++] = reportID;
    
    memcpy(&args[idx], buf, buf_len);
    
    uint8_t len = 4;
    union command *cmd = (union command *)IOMalloc(4 + args_len);
    memset(cmd, 0, 4+args_len);
    cmd->c.reg = this->HIDDescriptor.wCommandRegister;
    cmd->c.opcode = 0x03;
    cmd->c.reportTypeID = reportID | reportType << 4;
    
    uint8_t *rawCmd = (uint8_t *)cmd;
    
    memcpy(rawCmd + len, args, args_len);
    len += args_len;
    IOReturn ret = writeI2C(rawCmd, len);
    
    IOFree(cmd, 4+args_len);
    IOFree(args, args_len);
    return ret;
}

IOReturn VoodooI2CPrecisionTouchpad::enable_abs(){
    struct INPUT_MODE_FEATURE input_mode;
    input_mode.ReportID = INPUT_CONFIG_REPORT_ID;
    input_mode.Mode = INPUT_MODE_TOUCHPAD;
    input_mode.Reserved = 0x00;
    
    return write_feature(input_mode.ReportID, (uint8_t *)&input_mode, sizeof(struct INPUT_MODE_FEATURE));
}

void VoodooI2CPrecisionTouchpad::read_input(){
    //Adjust this function to suit your touchpad.
    
    uint16_t maxLen = this->HIDDescriptor.wMaxInputLength;
    
    uint8_t *report = (uint8_t *)IOMalloc(maxLen);
    memset(report, 0, maxLen);
    IOReturn ret = readI2C(report, maxLen);
    
    if (report[2] == TOUCHPAD_REPORT_ID){
        int return_size = report[0] | report[1] << 8;
        if (return_size - 2 != sizeof(TOUCHPAD_INPUT_REPORT)){
            report[2] = 0xff; //Invalidate Report ID so it's not parsed;
        }
        
        struct TOUCHPAD_INPUT_REPORT inputReport;
        memcpy(&inputReport, report + 2, sizeof(TOUCHPAD_INPUT_REPORT));
        
        uint8_t ContactID = inputReport.MTouch.ContactInfo >> 4;
        uint8_t ContactStatus = inputReport.MTouch.ContactInfo & 0xF;
        
        if (ContactID >= 0 && ContactID < 15){
            if (ContactStatus & CONFIDENCE_BIT){
                if (ContactStatus & TIPSWITCH_BIT){
                    uint32_t pos_x = inputReport.MTouch.XValue;
                    uint32_t pos_y = inputReport.MTouch.YValue;
                    
                    pos_x /= 3;
                    pos_y /= 3;
                    
                    sc.x[ContactID] = pos_x;
                    sc.y[ContactID] = pos_y;
                    sc.p[ContactID] = 10;
                } else {
                    sc.x[ContactID] = -1;
                    sc.y[ContactID] = -1;
                    sc.p[ContactID] = -1;
                }
            }
        }
        
        sc.buttondown = inputReport.Button & 0x1;
    }
    IOFree(report, maxLen);
    this->IsReading = false;
}

static void precisionTouchpad_readReport(VoodooI2CPrecisionTouchpad *device){
    device->read_input();
}

void VoodooI2CPrecisionTouchpad::InterruptOccured(OSObject* owner, IOInterruptEventSource* src, int intCount){
    if (this->IsReading)
        return;
    if (!this->DeviceIsAwake)
        return;
    
    this->IsReading = true;
    
    thread_t newThread;
    kern_return_t kr = kernel_thread_start((thread_continue_t)precisionTouchpad_readReport, this, &newThread);
    if (kr != KERN_SUCCESS){
        this->IsReading = false;
        IOLog("%s::Thread error!\n", getName());
    } else {
        thread_deallocate(newThread);
    }
}
