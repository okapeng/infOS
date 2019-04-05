/*
 * CMOS Real-time Clock
 * SKELETON IMPLEMENTATION -- TO BE FILLED IN FOR TASK (3)
 */

/*
 * STUDENT NUMBER: s1768094
 */
#include <infos/drivers/timer/rtc.h>
#include <infos/util/lock.h>
#include <arch/x86/pio.h>

using namespace infos::drivers;
using namespace infos::drivers::timer;
using namespace infos::util;
using namespace infos::arch::x86;


class CMOSRTC : public RTC {
public:
    static const DeviceClass CMOSRTCDeviceClass;

    const DeviceClass& device_class() const override
    {
        return CMOSRTCDeviceClass;
    }

    /**
     * Convert binary coded decimal to binary number
     * @param BCD Binary coded decial integer number(8bits)
     */
    void BCD_to_binary(unsigned short& BCD) {
        BCD = (BCD & 0x0F) + ((BCD >> 4) * 10);;
    }

    /**
     * Get the value of CMOS memory by givening the offset 
     * @param offset Offset of the CMOS memory location intended to read
     * @return The byte at the given CMOS memory offset
     */
    uint8_t get_register(uint8_t offset) {
        __outb(0x70, offset);
        return __inb(0x71);
    }

    /**
     * Wait for an update cycle to begin and then wait for it being finished
     */
    void update_in_process() {
        while((get_register(0xA) & 0x80) == 0);
        while (get_register(0xA) & 0x80);
    }

    /**
     * Read data from CMOS memory
     */
    void read_CMOS(RTCTimePoint& tp) {
        tp.day_of_month = get_register(0x07);
        tp.month = get_register(0x08);
        tp.year = get_register(0x09);
        tp.hours = get_register(0x04);
        tp.minutes = get_register(0x02);
        tp.seconds = get_register(0x00);
    }

    /**
     * Convert time in binary coded decimal into binary 
     */
    void BCD_time_to_binary_time(RTCTimePoint& tp) {
        BCD_to_binary(tp.seconds);
        BCD_to_binary(tp.minutes);
        // If hour is in 12 hour format and is pm i.e. the 8th bit is set, the higher bits should be masked
        tp.hours = ((tp.hours & 0x0F) + (((tp.hours & 0x70) / 16) * 10) ) | (tp.hours & 0x80);        
        BCD_to_binary(tp.day_of_month);
        BCD_to_binary(tp.month);
        BCD_to_binary(tp.year);
    }

    /**
     * Interrogates the RTC to read the current date & time.
     * @param tp Populates the tp structure with the current data & time, as
     * given by the CMOS RTC device.
     */
    void read_timepoint(RTCTimePoint& tp) override
    {
        // Diabled interrupt before access RTC
        UniqueIRQLock l;

        // Wait for the chip to complete a full RTC update cycle
        update_in_process();

        // Access the CMOS memory to read the value of RTC clock 
        read_CMOS(tp);
        
        // Read the status register B to determine if the format of the value is BCD or binary
        uint8_t reg_b = get_register(0x0B);
        bool is_binary = reg_b & 0x04;
        bool is_24 = reg_b & 0x02;

        // Convert the value of time into binary if it is in binary coded decimal
		if (!is_binary) {
			BCD_time_to_binary_time(tp);
		}

		// Convert the value of hour into 24 hour format if it is in 12 hour format
		if (is_24) {
			// Add 12 hours if the hour is pm i.e. 0x80 bit is set
			tp.hours = ((tp.hours & 0x7F) + 12*((tp.hours & 0x80)>>7)) % 24;
		}

    }
};

const DeviceClass CMOSRTC::CMOSRTCDeviceClass(RTC::RTCDeviceClass, "cmos-rtc");

RegisterDevice(CMOSRTC);
