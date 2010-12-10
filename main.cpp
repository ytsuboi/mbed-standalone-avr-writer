// AVR(328P) stand-alone writer.
// Written for Make: Tokyo meeting 06
// by @ytsuboi

#include "mbed.h"
// TextLCD could be found at http://mbed.org/users/simon/libraries/TextLCD/livod0
#include "TextLCD.h"

LocalFileSystem local("local");
TextLCD lcd(p24, p26, p27, p28, p29, p30);
SPI avrspi(p11, p12, p13); // mosi, miso, sck
DigitalOut avrrst(p14);
DigitalIn enable(p10);

int main(void) {
    char    str[100];

    lcd.cls();
    // Start screen
    lcd.locate(0, 0);
    lcd.printf("mbed Stand-Alone");
    lcd.locate(5, 1);
    lcd.printf("AVR writer");
    wait(3);

    lcd.locate(0, 0);
    lcd.printf("put your AVR and");
    lcd.locate(0, 1);
    lcd.printf("push to continue");
    
    while(1) {
        if(!enable) {
            break;
        }
        wait(0.25);
    }

    avrrst = 1;
    wait_ms(27);
    avrrst = 0;
    wait_ms(27);   

    // Setup the spi for 8 bit data, high steady state clock,
    // second edge capture, with a 100KHz clock rate
    
    avrspi.format(8,0);
    avrspi.frequency(100000);
    
    wait_ms(25);
    
    // enter program mode
    avrspi.write(0xAC);
    avrspi.write(0x53);
    int response = avrspi.write(0x00);
    avrspi.write(0x00);

    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Enter Prog mode:");
    lcd.locate(9, 1);
    if (response == 0x53) {
        lcd.printf("Success");
    } else {
        lcd.printf("Failed");
        return -1;
    }
    
    wait(2);

// Check Device Sig.
    //Vendor Code
    avrspi.write(0x30);
    avrspi.write(0x00);
    avrspi.write(0x00);
    int sig0 = avrspi.write(0x00);
    //Product Family Code and Flash Size
    avrspi.write(0x30);
    avrspi.write(0x00);
    avrspi.write(0x01);
    int sig1 = avrspi.write(0x00);
    //Part Number
    avrspi.write(0x30);
    avrspi.write(0x00);
    avrspi.write(0x02);
    int sig2 = avrspi.write(0x00);

    lcd.cls();
    lcd.locate(0, 0);
    sprintf(str, "Device: %02X %02X %02X", sig0, sig1, sig2);
    lcd.printf(str);
//    lcd.printf("Device:");
    lcd.locate(0, 1);
    if (sig0==0x1E && sig1==0x95 && sig2==0x0F) {
        lcd.printf("ATmega 328P");
    } else if (sig0==0x1E) {
        lcd.printf("Unsupport Atmel");
    return -1;
    } else if (sig0==0x00 || sig1==0x01 || sig2==0x02) {
        lcd.printf("LOCKED!!");
    return -1;
    } else if (sig0==0xFF || sig1==0xFF || sig2||0xFF) {
        lcd.printf("MISSING!!");
    return -1;
    } else {
        lcd.printf("UNKNOWN!!");
    return -1;
    }

    wait(2);
 
// Erase Flash   
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Erasing Flash...");
    avrspi.write(0xAC);
    avrspi.write(0x80);
    avrspi.write(0x00);
    avrspi.write(0x00);
    wait_ms(9);
    //poll
    do {
        avrspi.write(0xF0);
        avrspi.write(0x00);
        avrspi.write(0x00);
        response = avrspi.write(0x00);
        } while ((response & 0x01) != 0);
    //end poll
    lcd.locate(10, 1);
    lcd.printf("DONE");
    
    wait(2);
    
// Open binary file
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Opening file...");
    FILE *fp = fopen("/local/AVRCODE.bin", "rb");

    if (fp == NULL) {
        lcd.locate(0, 1);
        lcd.printf("Failed to open!!");
        return -1;
    } else {
        lcd.locate(0, 1);
        lcd.printf("Opened!!");
        wait(2);
    
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Writing binaries");
// reset AVR
//    avrrst = 1;
//    wait_ms(27);
//    avrrst = 0;
//    wait_ms(27);

    int  pageOffset = 0;
    int  pageNum = 0;
    int  n          = 0;
    int  HighLow    = 0;

// do programing
    while ((n = getc(fp)) != EOF) {
        //write loaded page to flash
        //Page size of 328P is 64word/page
        if (pageOffset == 64) {
            avrspi.write(0x4C);
            avrspi.write((pageNum >> 2) & 0x3F);
            avrspi.write((pageNum & 0x03) << 6);
            avrspi.write(0x00);
            wait_ms(5);
            //poll
            do {
                avrspi.write(0xF0);
                avrspi.write(0x00);
                avrspi.write(0x00);
                response = avrspi.write(0x00);
                } while ((response & 0x01) != 0);
            //end poll
            pageNum++;
            // Total page of 328P is 256page
            if (pageNum > 256) {
                break;
            }
            
            pageOffset = 0;
        }

        // load low byte
        if (HighLow == 0) {
            avrspi.write(0x40);
            avrspi.write(0x00);
            avrspi.write(pageOffset & 0x3F);
            avrspi.write(n);

            //poll
            do {
                avrspi.write(0xF0);
                avrspi.write(0x00);
                avrspi.write(0x00);
                response = avrspi.write(0x00);
                } while ((response & 0x01) != 0);
            //end poll

            HighLow = 1;
        }
        // load high byte
        else {
            avrspi.write(0x48);
            avrspi.write(0x00);
            avrspi.write(pageOffset & 0x3F);
            avrspi.write(n);

            //poll
            do {
                avrspi.write(0xF0);
                avrspi.write(0x00);
                avrspi.write(0x00);
                response = avrspi.write(0x00);
                } while ((response & 0x01) != 0);
            //end poll

            HighLow = 0;
            pageOffset++;
        }
    }

//close binary file
    fclose(fp);

    lcd.locate(0, 1);
    lcd.printf("DONE!!");
    wait(2);
    }

//write low fuse bit
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Writing Low Fuse");
    avrspi.write(0xAC);
    avrspi.write(0xA0);
    avrspi.write(0x00);
    avrspi.write(0xE2);
    wait_ms(5);
    //poll
    do {
        avrspi.write(0xF0);
        avrspi.write(0x00);
        avrspi.write(0x00);
        response = avrspi.write(0x00);
        } while ((response & 0x01) != 0);
    //end poll
    lcd.locate(10, 1);
    lcd.printf("DONE");
    wait(1);

//write high fuse bit
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Writing Hi Fuse");
    avrspi.write(0xAC);
    avrspi.write(0xA8);
    avrspi.write(0x00);
    avrspi.write(0xDA);
    wait_ms(5);
    //poll
    do {
        avrspi.write(0xF0);
        avrspi.write(0x00);
        avrspi.write(0x00);
        response = avrspi.write(0x00);
        } while ((response & 0x01) != 0);
    //end poll
    lcd.locate(10, 1);
    lcd.printf("DONE");
    wait(1);

//write ext fuse bit
    lcd.cls();
    lcd.locate(0, 0);
    lcd.printf("Writing Ext Fuse");
    avrspi.write(0xAC);
    avrspi.write(0xA4);
    avrspi.write(0x00);
    avrspi.write(0x05);
    wait_ms(5);
    //poll
    do {
        avrspi.write(0xF0);
        avrspi.write(0x00);
        avrspi.write(0x00);
        response = avrspi.write(0x00);
        } while ((response & 0x01) != 0);
    //end poll
    lcd.locate(10, 1);
    lcd.printf("DONE");
    wait(1);

// exit program mode
     avrrst = 1;
// end
    return 0;
}

