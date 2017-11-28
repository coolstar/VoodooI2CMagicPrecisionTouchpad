#include "CSMagicMultitouchDevice.hpp"
#include "VoodooI2CPrecisionTouchpad.hpp"

#define super IOHIDDevice

#define REPORTID_HELLO 0
#define REPORTID_INFO 1
#define REPORTID_MTOUCH 2
#define REPORTID_DESC 219

static unsigned char MagicTrackpadInputReport[110] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x02,        // Usage (Mouse)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x03,        //     Usage Maximum (0x03)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x85, 0x02,        //     Report ID (2)
    0x95, 0x03,        //     Report Count (3)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x05,        //     Report Size (5)
    0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x81,        //     Logical Minimum (-127)
    0x25, 0x7F,        //     Logical Maximum (127)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x02,        //     Report Count (2)
    0x81, 0x06,        //     Input (Data,Var,Rel,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x04,        //     Report Count (4)
    0x75, 0x08,        //     Report Size (8)
    0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xC0,              // End Collection
    0x05, 0x0D,        // Usage Page (Digitizer)
    0x09, 0x05,        // Usage (Touch Pad)
    0xA1, 0x01,        // Collection (Application)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x0C,        //   Usage (0x0C)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x10,        //   Report Count (16)
    0x85, 0x3F,        //   Report ID (63)
    0x81, 0x22,        //   Input (Data,Var,Abs,No Wrap,Linear,No Preferred State,No Null Position)
    0xC0,              // End Collection
    0x06, 0x00, 0xFF,  // Usage Page (Vendor Defined 0xFF00)
    0x09, 0x0C,        // Usage (0x0C)
    0xA1, 0x01,        // Collection (Application)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x09, 0x0C,        //   Usage (0x0C)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x85, 0x44,        //   Report ID (68)
    0x75, 0x08,        //   Report Size (8)
    0x96, 0x6B, 0x05,  //   Report Count (1387)
    0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
    
    // 110 bytes
};

struct __attribute__((__packed__)) MAGIC_TRACKPAD_INPUT_REPORT_FINGER {
    UInt8 AbsX;
    UInt8 AbsXY;
    UInt8 AbsY[2];
    UInt8 Touch_Major;
    UInt8 Touch_Minor;
    UInt8 Size;
    UInt8 Pressure;
    UInt8 Orientation_Origin;
};

struct __attribute__((__packed__)) MAGIC_TRACKPAD_INPUT_REPORT {
    UInt8 ReportID;
    UInt8 Button;
    UInt16 Unused[2];
    
    UInt16 TouchActive; //2 if not active, 3 if active
    
    UInt32 Timestamp; //must end in 0x31
    
    MAGIC_TRACKPAD_INPUT_REPORT_FINGER FINGERS[12]; //May support more fingers
};

OSDefineMetaClassAndStructors(CSMagicMultitouchDevice, IOHIDDevice)

bool CSMagicMultitouchDevice::start(IOService *provider){
    reg_off1 = 0xDB;
    if (super::start(provider)){
        registerService();
        return true;
    }
    return false;
}

IOReturn CSMagicMultitouchDevice::newReportDescriptor(IOMemoryDescriptor **descriptor) const {
    IOBufferMemoryDescriptor *buffer = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, 0, sizeof(MagicTrackpadInputReport));
    if (!buffer)
        return kIOReturnNoResources;
    
    buffer->writeBytes(0, MagicTrackpadInputReport, sizeof(MagicTrackpadInputReport));
    *descriptor = buffer;
    return kIOReturnSuccess;
}

IOReturn CSMagicMultitouchDevice::setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options){
    UInt8 reportID = options & 0xFF;
    
    IOLog("MagicTouch::setReport type=%d, id=%d\n", reportType, reportID);
    
    if (reportType == kIOHIDReportTypeFeature){
        if (report->getLength() == 2){
            if (reportID == REPORTID_INFO){
                char feature_report[2];
                report->readBytes(0, feature_report, 2);
                
                reg_off1 = feature_report[1];
                
                IOLog("MagicTouch::setReport requested reg %d\n", reg_off1);
                
                return kIOReturnSuccess;
            } else if (reportID == REPORTID_MTOUCH){
                //byte 1 = enable
                return kIOReturnSuccess;
            } else if (reportID == 200){
                return kIOReturnSuccess;
            }
        }
    } else if (reportType == kIOHIDReportTypeOutput){
        return kIOReturnUnsupported;
    }
    return kIOReturnUnsupported;
}

IOReturn CSMagicMultitouchDevice::getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options){
    UInt8 reportID = options & 0xFF;
    
    IOLog("MagicTouch::getReport type=%d, id=%d\n", reportType, reportID);
    
    if (reportType == kIOHIDReportTypeFeature){
        if (reportID == REPORTID_HELLO){
            unsigned char hello_packet[2] = {0x00, 0x01};
            report->writeBytes(0, hello_packet, 2);
            return kIOReturnSuccess;
        } else if (reportID == REPORTID_INFO){
            if (reg_off1 == 0xDB){ //TODO: FIGURE OUT WHAT THESE DO
                unsigned char info_report[5] = {0x01, 0xDB, 0x00, 0x49, 0x00};
                report->writeBytes(0, info_report, 5);
                return kIOReturnSuccess;
            } else if (reg_off1 == 0xC8){
                unsigned char info_report[5] = {0x01, 0xC8, 0x00, 0x01, 0x00};
                report->writeBytes(0, info_report, 5);
                return kIOReturnSuccess;
            }
        } else if (reportID == REPORTID_DESC){ //TODO: Figure out what this does
            unsigned char report_desc[74] = {0xDB, 0x01, 0x02, 0x00, 0xD1, 0x81, 0x0D, 0x00, 0xD3, 0x01, 0x16, 0x1E, 0x03, 0x95, 0x00, 0x14, 0x1E, 0x62, 0x05, 0x00, 0x00, 0x10, 0x00, 0xD0, 02, 0x01, 0x00, 0x14, 0x01, 00, 0x1E, 0x00,
                0x02, 0x14, 0x02, 0x01, 0x0E, 0x02, 0x00, 0x07, 0x00, 0xA1, 0x00, 0x00, 0x05, 0x00, 0xFE, 0x01, 0x11, 0x00, 0xD9, 0xF0, 0x3C, 0x00, 0x00, 0x20, 0x2B, 0x00, 0x00, 0x44, 0xE3, 0x52, 0xFF, 0xBD,
                0x1E, 0xE4, 0x26, 0x05, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00};
            report->writeBytes(0, report_desc, 74);
            return kIOReturnSuccess;
        } else if (reportID == 200){
            unsigned char unknown_report[2] = {0xC8, 0x09};
            report->writeBytes(0, unknown_report, 2);
            return kIOReturnSuccess;
        }
        return kIOReturnUnsupported;
    } else if (reportType == kIOHIDReportTypeInput){
        return kIOReturnUnsupported;
    }
    return kIOReturnUnsupported;
}

IOReturn CSMagicMultitouchDevice::handleInput(magic_softc *softc){
    uint8_t finger_count = 0;
    for (int i = 0; i < MAX_FINGERS; i++){
        if (softc->x[i] != -1){
            finger_count++;
        }
    }
    
    uint8_t byte_count = 12 + (finger_count * 9);
    uint8_t *data = (uint8_t *)IOMalloc(byte_count);
    
    MAGIC_TRACKPAD_INPUT_REPORT *inputReport = (MAGIC_TRACKPAD_INPUT_REPORT *)data;
    
    inputReport->ReportID = 0x02;
    inputReport->Button = softc->buttondown ? 0x01 : 0x00;
    
    if (finger_count > 0)
        data[7] = 0x03;
    else
        data[7] = 0x02;
    
    data[8] = 0x31; //Magic
    
    int timestamp = 31580;
    data[11] = (timestamp >> 13) & 0xFF;
    data[10] = (timestamp >> 5) & 0xFF;
    data[9] = timestamp & 0x1F;
    
    int j = 0;
    for (int i = 0; i < MAX_FINGERS; i++){
        if (softc->x[i] != -1){
            MAGIC_TRACKPAD_INPUT_REPORT_FINGER *finger = &inputReport->FINGERS[j];
            
            int16_t x = softc->x[i];
            int16_t y = softc->y[i];
            
            int16_t x_min = -3678;
            int16_t y_min = -2479;
            
            finger->AbsX = (x << 19) >> 19;
            finger->AbsXY = ((x << 19) >> 27);
            
            y = -y;
            
            finger->AbsXY |= ((y << 19) >> 14);
            
            finger->AbsY[0] = ((y << 19) >> 22);
            finger->AbsY[1] = ((y << 19) >> 30);
            
            finger->Touch_Major = 121;
            finger->Touch_Minor = 120;
            finger->Size = 30;
            finger->Pressure = 10;
            
            finger->Orientation_Origin = i;
            
            finger->Orientation_Origin += 0x80;
            
            j++;
        }
    }
    
    
    IOBufferMemoryDescriptor *report = IOBufferMemoryDescriptor::inTaskWithOptions(kernel_task, 0, byte_count);
    report->writeBytes(0, data, byte_count);
    
    IOFree(data, byte_count);
    
    return this->handleReport(report);
}

OSNumber *CSMagicMultitouchDevice::newVendorIDNumber() const {
    return OSNumber::withNumber(0x05ac, 32);
}

OSNumber *CSMagicMultitouchDevice::newProductIDNumber() const {
    return OSNumber::withNumber(0x0265, 32);
}

OSString *CSMagicMultitouchDevice::newProductString() const {
    return OSString::withCString("Magic Trackpad 2");
}

OSString *CSMagicMultitouchDevice::newManufacturerString() const {
    return OSString::withCString("Apple Inc.");
}

OSNumber *CSMagicMultitouchDevice::newPrimaryUsageNumber() const {
    return OSNumber::withNumber(kHIDUsage_GD_Mouse, 32);
}

OSNumber *CSMagicMultitouchDevice::newPrimaryUsagePageNumber() const {
    return OSNumber::withNumber(kHIDPage_GenericDesktop, 32);
}

OSString *CSMagicMultitouchDevice::newTransportString() const {
    return OSString::withCString("USB");
}
