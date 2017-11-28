#ifndef CSMagicMultitouchDevice_hpp
#define CSMagicMultitouchDevice_hpp

#include <IOKit/IOService.h>
#include <IOKit/hid/IOHIDDevice.h>

#define MAX_FINGERS 5

struct magic_softc {
    int16_t x[MAX_FINGERS];
    int16_t y[MAX_FINGERS];
    int16_t p[MAX_FINGERS];
    
    bool buttondown;
};

class CSMagicMultitouchDevice : public IOHIDDevice {
    OSDeclareDefaultStructors(CSMagicMultitouchDevice)
public:
    virtual bool start(IOService *provider) override;
    virtual IOReturn newReportDescriptor(IOMemoryDescriptor **descriptor) const override;
    virtual IOReturn setReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options) override;
    virtual IOReturn getReport(IOMemoryDescriptor *report, IOHIDReportType reportType, IOOptionBits options) override;
    
    virtual OSNumber* newVendorIDNumber() const override;
    virtual OSNumber* newProductIDNumber() const override;
    virtual OSString* newProductString() const override;
    virtual OSString* newManufacturerString() const override;
    
    virtual OSNumber* newPrimaryUsageNumber() const override;
    virtual OSNumber* newPrimaryUsagePageNumber() const override;
    
    virtual OSString* newTransportString() const override;
    
    virtual IOReturn handleInput(magic_softc *softc);
private:
    unsigned char reg_off1;
};

#endif
